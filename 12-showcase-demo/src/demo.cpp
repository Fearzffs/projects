#include "showcase/demo.hpp"
#include "showcase/metrics.hpp"

#include "arena_allocator/arena.hpp"
#include "async_logger/async_logger.hpp"
#include "blocking_mpmc_queue/blocking_mpmc_queue.hpp"
#include "event_bus/event_bus.hpp"
#include "fsm/fsm.hpp"
#include "ring_buffer/ring_buffer.hpp"
#include "signal_slot/signal.hpp"
#include "task_graph/task_graph.hpp"
#include "thread_pool/thread_pool.hpp"
#include "timer_scheduler/timer_scheduler.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace showcase {

enum class State { Boot, Running, Shutdown };
enum class Event { Start, Quit };

namespace {

struct StatsReady {
    WindowStats stats;
};

struct App {
    DemoConfig config;

    klib::ThreadPool pool{4};
    klib::AsyncLogger logger{512};
    klib::EventBus bus{pool};
    klib::TimerScheduler timers{pool};
    klib::BlockingMpmcQueue<Sample> samples{128};
    klib::RingBuffer<Sample> history{64};
    klib::Arena arena{8 * 1024};
    klib::Fsm<State, Event> fsm;
    klib::Signal<WindowStats> stats_updated;
    klib::Connection stats_conn;
    klib::Subscription bus_sub;
    klib::TimerHandle sample_timer;
    klib::TimerHandle quit_timer;

    std::shared_ptr<klib::TaskGraph> active_graph;
    MetricsSampler sampler;

    std::mutex event_mu;
    std::vector<Event> pending_events;

    std::atomic<int> samples_since_report{0};
    std::atomic<std::uint64_t> drops{0};
    std::atomic<int> reports_completed{0};
    std::atomic<bool> flush_busy{false};
    std::atomic<bool> done{false};

    int report_index{0};
    WindowStats last_stats{};

    std::mutex batch_mu;
    std::vector<Sample> batch;

    void post(Event e) {
        std::lock_guard lock(event_mu);
        pending_events.push_back(e);
    }

    void drain_events() {
        std::vector<Event> local;
        {
            std::lock_guard lock(event_mu);
            local.swap(pending_events);
        }
        for (Event e : local) {
            fsm.handle(e);
        }
    }

    void log_info(std::string msg) {
        (void)logger.try_log(klib::LogLevel::info, std::move(msg));
    }

    void publish_report(const WindowStats& st) {
        (void)bus.publish(StatsReady{st});
        stats_updated.emit(st);
        reports_completed.fetch_add(1, std::memory_order_relaxed);
    }

    /// Drain + aggregate + report without leaving Running.
    void start_flush_graph() {
        bool expected = false;
        if (!flush_busy.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return;
        }

        {
            std::lock_guard lock(batch_mu);
            batch.clear();
            while (auto s = samples.try_pop()) {
                batch.push_back(std::move(*s));
            }
            if (batch.empty()) {
                flush_busy.store(false, std::memory_order_release);
                return;
            }
            report_index = reports_completed.load(std::memory_order_relaxed) + 1;
        }

        active_graph = std::make_shared<klib::TaskGraph>(pool);
        auto& graph = *active_graph;

        auto drain_done = graph.add([] {});
        auto aggregate = graph.add([this] {
            std::lock_guard lock(batch_mu);
            arena.reset();
            (void)arena.try_allocate(64, alignof(std::max_align_t));

            WindowStats st;
            st.window_index = report_index;
            const std::size_t n = batch.size();
            st.samples = static_cast<int>(n);
            double sum_cpu = 0;
            double sum_load = 0;
            for (const auto& s : batch) {
                sum_cpu += s.cpu_percent;
                sum_load += s.load_1m;
                if (s.cpu_temp_c) {
                    st.last_temp_c = s.cpu_temp_c;
                }
            }
            if (n > 0) {
                st.avg_cpu_percent = sum_cpu / static_cast<double>(n);
                st.avg_load_1m = sum_load / static_cast<double>(n);
            }
            last_stats = st;
        });
        auto finalize = graph.add([this] { publish_report(last_stats); });
        graph.precede(drain_done, aggregate);
        graph.precede(aggregate, finalize);
        graph.set_on_complete([this] {
            flush_busy.store(false, std::memory_order_release);
        });
        (void)graph.try_run();
    }
};

void print_stats_line(const WindowStats& st) {
    std::cout << "cpu%=" << st.avg_cpu_percent << " load=" << st.avg_load_1m;
    if (st.last_temp_c) {
        std::cout << " tempC=" << *st.last_temp_c;
    }
    std::cout << '\n' << std::flush;
}

void setup_fsm(App& app) {
    app.fsm.add_transition(State::Boot, Event::Start, State::Running);
    app.fsm.add_transition(State::Running, Event::Quit, State::Shutdown);

    app.fsm.on_enter(State::Boot, [&app] {
        app.log_info("FSM Boot");
        app.post(Event::Start);
    });

    app.fsm.on_enter(State::Running, [&app] {
        app.log_info("FSM Running");
        app.samples_since_report.store(0, std::memory_order_relaxed);

        app.sample_timer = app.timers.try_run_every(
            std::chrono::milliseconds(app.config.sample_period_ms), [&app] {
                const auto sample =
                    app.sampler.take(app.samples.size(), app.drops.load(std::memory_order_relaxed));
                (void)app.history.try_push(sample);
                if (!app.samples.try_push(sample)) {
                    app.drops.fetch_add(1, std::memory_order_relaxed);
                }

                WindowStats snap;
                snap.window_index = app.reports_completed.load(std::memory_order_relaxed);
                snap.samples = 1;
                snap.avg_cpu_percent = sample.cpu_percent;
                snap.avg_load_1m = sample.load_1m;
                snap.last_temp_c = sample.cpu_temp_c;
                app.stats_updated.emit(snap);

                const int n =
                    app.samples_since_report.fetch_add(1, std::memory_order_relaxed) + 1;
                if (n >= app.config.report_every_samples) {
                    app.samples_since_report.store(0, std::memory_order_relaxed);
                    app.start_flush_graph();
                }
            });

        // 0 = run forever; otherwise quit after N seconds.
        if (app.config.run_seconds > 0) {
            app.quit_timer = app.timers.try_run_after(
                std::chrono::seconds(app.config.run_seconds),
                [&app] { app.post(Event::Quit); });
        }
    });

    app.fsm.on_exit(State::Running, [&app] {
        app.sample_timer.reset();
        app.quit_timer.reset();
    });

    app.fsm.on_enter(State::Shutdown, [&app] {
        app.log_info("FSM Shutdown");
        app.sample_timer.reset();
        app.quit_timer.reset();
        app.done.store(true, std::memory_order_release);
    });
}

}  // namespace

int run_demo(const DemoConfig& config) {
    App app;
    app.config = config;

    // One line on main via Queued signal (cpu / load / temp only).
    app.stats_conn =
        app.stats_updated.connect(&print_stats_line, klib::ConnectionType::Queued);

    app.bus_sub = app.bus.subscribe<StatsReady>([&app](const StatsReady& e) {
        (void)app.logger.try_log(klib::LogLevel::debug,
                                 "bus StatsReady report#" + std::to_string(e.stats.window_index));
    });

    setup_fsm(app);
    app.fsm.start(State::Boot);

    while (!app.done.load(std::memory_order_acquire)) {
        app.drain_events();
        (void)klib::SlotDispatcher::this_thread().process();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    app.drain_events();
    (void)klib::SlotDispatcher::this_thread().process();

    for (int i = 0; i < 50 && app.flush_busy.load(std::memory_order_acquire); ++i) {
        (void)klib::SlotDispatcher::this_thread().process();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    while (app.history.try_pop()) {
    }

    app.timers.shutdown();
    app.samples.shutdown();
    app.logger.shutdown();
    app.pool.shutdown();

    return app.done.load() ? 0 : 1;
}

}  // namespace showcase
