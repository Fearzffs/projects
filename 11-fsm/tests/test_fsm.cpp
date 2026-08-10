#include "fsm/fsm.hpp"

#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

namespace {

enum class State { Boot, Loading, Ready, Shutdown };
enum class Event { StartLoad, LoadDone, Quit, Unknown };

}  // namespace

namespace std {

template <>
struct hash<State> {
    size_t operator()(State s) const noexcept {
        return static_cast<size_t>(s);
    }
};

template <>
struct hash<Event> {
    size_t operator()(Event e) const noexcept {
        return static_cast<size_t>(e);
    }
};

}  // namespace std

TEST(Fsm, StartEntersInitialState) {
    klib::Fsm<State, Event> fsm;
    std::vector<State> entered;

    fsm.on_enter(State::Boot, [&] { entered.push_back(State::Boot); });
    fsm.start(State::Boot);

    EXPECT_TRUE(fsm.started());
    EXPECT_EQ(fsm.current(), State::Boot);
    ASSERT_EQ(entered.size(), 1u);
    EXPECT_EQ(entered[0], State::Boot);
}

TEST(Fsm, TransitionRunsExitThenEnter) {
    klib::Fsm<State, Event> fsm;
    std::vector<const char*> order;

    fsm.add_transition(State::Boot, Event::StartLoad, State::Loading);
    fsm.on_exit(State::Boot, [&] { order.push_back("exit_boot"); });
    fsm.on_enter(State::Loading, [&] { order.push_back("enter_loading"); });

    fsm.start(State::Boot);
    order.clear();  // ignore initial enter

    fsm.handle(Event::StartLoad);
    ASSERT_EQ(order.size(), 2u);
    EXPECT_STREQ(order[0], "exit_boot");
    EXPECT_STREQ(order[1], "enter_loading");
    EXPECT_EQ(fsm.current(), State::Loading);
}

TEST(Fsm, UnknownEventIsIgnored) {
    klib::Fsm<State, Event> fsm;
    int exits = 0;
    fsm.add_transition(State::Boot, Event::StartLoad, State::Loading);
    fsm.on_exit(State::Boot, [&] { ++exits; });
    fsm.start(State::Boot);

    fsm.handle(Event::Unknown);
    EXPECT_EQ(fsm.current(), State::Boot);
    EXPECT_EQ(exits, 0);
}

TEST(Fsm, FullPathBootToShutdown) {
    klib::Fsm<State, Event> fsm;
    fsm.add_transition(State::Boot, Event::StartLoad, State::Loading);
    fsm.add_transition(State::Loading, Event::LoadDone, State::Ready);
    fsm.add_transition(State::Ready, Event::Quit, State::Shutdown);

    fsm.start(State::Boot);
    fsm.handle(Event::StartLoad);
    EXPECT_EQ(fsm.current(), State::Loading);
    fsm.handle(Event::LoadDone);
    EXPECT_EQ(fsm.current(), State::Ready);
    fsm.handle(Event::Quit);
    EXPECT_EQ(fsm.current(), State::Shutdown);
}

TEST(Fsm, SelfTransitionRunsExitAndEnter) {
    klib::Fsm<State, Event> fsm;
    int exits = 0;
    int enters = 0;
    fsm.add_transition(State::Ready, Event::LoadDone, State::Ready);
    fsm.on_exit(State::Ready, [&] { ++exits; });
    fsm.on_enter(State::Ready, [&] { ++enters; });

    fsm.start(State::Ready);
    EXPECT_EQ(enters, 1);
    fsm.handle(Event::LoadDone);
    EXPECT_EQ(exits, 1);
    EXPECT_EQ(enters, 2);
    EXPECT_EQ(fsm.current(), State::Ready);
}

TEST(Fsm, DoubleStartThrows) {
    klib::Fsm<State, Event> fsm;
    fsm.start(State::Boot);
    EXPECT_THROW(fsm.start(State::Loading), std::logic_error);
}

TEST(Fsm, HandleBeforeStartThrows) {
    klib::Fsm<State, Event> fsm;
    EXPECT_THROW(fsm.handle(Event::StartLoad), std::logic_error);
}

TEST(Fsm, CurrentBeforeStartThrows) {
    klib::Fsm<State, Event> fsm;
    EXPECT_THROW((void)fsm.current(), std::logic_error);
}

TEST(Fsm, EnterExceptionDoesNotBreakMachine) {
    klib::Fsm<State, Event> fsm;
    fsm.add_transition(State::Boot, Event::StartLoad, State::Loading);
    fsm.on_enter(State::Loading, [] { throw std::runtime_error("boom"); });
    fsm.start(State::Boot);
    fsm.handle(Event::StartLoad);
    EXPECT_EQ(fsm.current(), State::Loading);
}
