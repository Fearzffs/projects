#include "signal_slot/signal.hpp"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace std::chrono_literals;

namespace {

struct Hud {
    std::vector<int> hits;

    void on_health(int hp) { hits.push_back(hp); }
};

}  // namespace

TEST(Signal, EmitInvokesSlotsInConnectOrder) {
    klib::Signal<int> sig;
    std::vector<int> order;

    auto c1 = sig.connect([&](int v) { order.push_back(v * 10 + 1); });
    auto c2 = sig.connect([&](int v) { order.push_back(v * 10 + 2); });

    sig.emit(3);
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 31);
    EXPECT_EQ(order[1], 32);
    EXPECT_EQ(sig.slot_count(), 2u);
    (void)c1;
    (void)c2;
}

TEST(Signal, MemberBindReceivesArgs) {
    klib::Signal<int> sig;
    Hud hud;
    auto c = sig.connect(&hud, &Hud::on_health);

    sig.emit(80);
    sig.emit(40);
    ASSERT_EQ(hud.hits.size(), 2u);
    EXPECT_EQ(hud.hits[0], 80);
    EXPECT_EQ(hud.hits[1], 40);
    (void)c;
}

TEST(Signal, DisconnectStopsDelivery) {
    klib::Signal<> sig;
    int hits = 0;
    auto c = sig.connect([&] { ++hits; });

    sig.emit();
    EXPECT_EQ(hits, 1);
    c.reset();
    sig.emit();
    EXPECT_EQ(hits, 1);
    EXPECT_EQ(sig.slot_count(), 0u);
}

TEST(Signal, ConnectDuringEmitMissesCurrentEmission) {
    klib::Signal<> sig;
    int a_hits = 0;
    int b_hits = 0;
    klib::Connection late;

    auto c = sig.connect([&] {
        ++a_hits;
        late = sig.connect([&] { ++b_hits; });
    });

    sig.emit();
    EXPECT_EQ(a_hits, 1);
    EXPECT_EQ(b_hits, 0);

    sig.emit();
    EXPECT_EQ(a_hits, 2);
    EXPECT_EQ(b_hits, 1);
    (void)c;
}

TEST(Signal, MultiArgEmit) {
    klib::Signal<int, std::string> sig;
    int got_i = 0;
    std::string got_s;
    auto c = sig.connect([&](int i, const std::string& s) {
        got_i = i;
        got_s = s;
    });

    sig.emit(7, "ok");
    EXPECT_EQ(got_i, 7);
    EXPECT_EQ(got_s, "ok");
    (void)c;
}

TEST(Signal, EmptyEmitIsNoOp) {
    klib::Signal<int> sig;
    EXPECT_EQ(sig.slot_count(), 0u);
    sig.emit(1);  // must not crash
}

TEST(Signal, NullObjectThrows) {
    klib::Signal<int> sig;
    Hud* hud = nullptr;
    EXPECT_THROW(sig.connect(hud, &Hud::on_health), std::invalid_argument);
}

TEST(Signal, ExceptionInSlotDoesNotStopOthers) {
    klib::Signal<> sig;
    int hits = 0;
    auto c1 = sig.connect([] { throw std::runtime_error("boom"); });
    auto c2 = sig.connect([&] { ++hits; });

    sig.emit();
    EXPECT_EQ(hits, 1);
    (void)c1;
    (void)c2;
}

TEST(Signal, DirectRunsOnEmitterThread) {
    klib::Signal<> sig;
    std::thread::id slot_thread;
    const auto emitter = std::this_thread::get_id();

    auto c = sig.connect(
        [&] { slot_thread = std::this_thread::get_id(); },
        klib::ConnectionType::Direct);

    sig.emit();
    EXPECT_EQ(slot_thread, emitter);
    (void)c;
}

TEST(Signal, QueuedRunsOnConnectThreadAfterProcess) {
    klib::Signal<int> sig;
    std::atomic<bool> connected{false};
    std::atomic<bool> emitted{false};
    std::atomic<bool> done{false};
    std::thread::id connect_thread;
    std::thread::id slot_thread;
    int value = 0;
    klib::Connection conn;

    std::thread worker([&] {
        connect_thread = std::this_thread::get_id();
        conn = sig.connect(
            [&](int v) {
                value = v;
                slot_thread = std::this_thread::get_id();
                done.store(true, std::memory_order_release);
            },
            klib::ConnectionType::Queued);
        connected.store(true, std::memory_order_release);

        while (!emitted.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(1ms);
        }
        // Slot must not have run yet — still queued on this thread.
        EXPECT_FALSE(done.load(std::memory_order_acquire));
        EXPECT_EQ(klib::SlotDispatcher::this_thread().pending(), 1u);

        const std::size_t ran = klib::SlotDispatcher::this_thread().process();
        EXPECT_EQ(ran, 1u);
        EXPECT_TRUE(done.load(std::memory_order_acquire));
    });

    while (!connected.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(1ms);
    }
    sig.emit(42);
    emitted.store(true, std::memory_order_release);

    worker.join();
    EXPECT_EQ(value, 42);
    EXPECT_EQ(slot_thread, connect_thread);
    EXPECT_NE(slot_thread, std::this_thread::get_id());
}

TEST(Signal, QueuedSameThreadStillNeedsProcess) {
    klib::Signal<> sig;
    int hits = 0;
    auto c = sig.connect([&] { ++hits; }, klib::ConnectionType::Queued);

    sig.emit();
    EXPECT_EQ(hits, 0);
    EXPECT_EQ(klib::SlotDispatcher::this_thread().pending(), 1u);

    EXPECT_EQ(klib::SlotDispatcher::this_thread().process(), 1u);
    EXPECT_EQ(hits, 1);
    (void)c;
}
