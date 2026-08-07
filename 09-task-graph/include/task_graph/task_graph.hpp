#pragma once

#include "thread_pool/thread_pool.hpp"

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace portfolio {
namespace detail {

struct TaskGraphState {
    struct Node {
        std::function<void()> work;
        std::function<void()> on_done;
        std::size_t deps_remaining{0};
        std::vector<std::uint64_t> successors;
        bool submitted{false};
    };

    explicit TaskGraphState(ThreadPool& pool) : pool_(pool) {}

    ThreadPool& pool() noexcept { return pool_; }

    std::uint64_t add(std::function<void()> work, std::function<void()> on_done) {
        std::lock_guard lock(mutex_);
        if (started_) {
            throw std::logic_error("TaskGraph::add after try_run");
        }
        const std::uint64_t id = nodes_.size();
        nodes_.push_back(Node{std::move(work), std::move(on_done), 0, {}, false});
        ++remaining_;
        return id;
    }

    void precede(std::uint64_t before, std::uint64_t after) {
        std::lock_guard lock(mutex_);
        if (started_) {
            throw std::logic_error("TaskGraph::precede after try_run");
        }
        if (before >= nodes_.size() || after >= nodes_.size()) {
            throw std::out_of_range("TaskGraph::precede invalid task id");
        }
        if (before == after) {
            throw std::invalid_argument("TaskGraph::precede: task cannot depend on itself");
        }
        nodes_[before].successors.push_back(after);
        ++nodes_[after].deps_remaining;
    }

    void set_on_complete(std::function<void()> cb) {
        std::lock_guard lock(mutex_);
        on_complete_ = std::move(cb);
    }

    /// Submit every task that currently has zero remaining deps.
    /// Returns false if any try_submit to the pool failed.
    bool try_run(const std::shared_ptr<TaskGraphState>& self) {
        std::vector<std::uint64_t> ready;
        std::function<void()> complete_now;
        {
            std::lock_guard lock(mutex_);
            if (stopping_) {
                return false;
            }
            started_ = true;
            if (nodes_.empty()) {
                complete_now = on_complete_;
                on_complete_ = nullptr;
            } else {
                for (std::uint64_t id = 0; id < nodes_.size(); ++id) {
                    if (nodes_[id].deps_remaining == 0 && !nodes_[id].submitted) {
                        ready.push_back(id);
                    }
                }
            }
        }

        if (complete_now) {
            complete_now();
            return true;
        }

        bool all_ok = true;
        for (std::uint64_t id : ready) {
            if (!submit_node(self, id)) {
                all_ok = false;
            }
        }
        return all_ok;
    }

    void request_stop() {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }

    /// Test helper: block until all tasks have finished (or graph was empty).
    void wait() {
        std::unique_lock lock(mutex_);
        done_cv_.wait(lock, [this] { return remaining_ == 0 || stopping_; });
    }

private:
    bool submit_node(const std::shared_ptr<TaskGraphState>& self, std::uint64_t id) {
        std::function<void()> work;
        std::function<void()> on_done;
        {
            std::lock_guard lock(mutex_);
            if (stopping_ || id >= nodes_.size()) {
                return false;
            }
            auto& node = nodes_[id];
            if (node.submitted || node.deps_remaining != 0) {
                return true;  // not an error — already handled / not ready
            }
            node.submitted = true;
            work = std::move(node.work);
            on_done = std::move(node.on_done);
        }

        const bool ok = pool_.try_submit(
            [self, id, work = std::move(work), on_done = std::move(on_done)]() mutable {
                try {
                    if (work) {
                        work();
                    }
                } catch (...) {
                    // Keep the graph moving; same spirit as ThreadPool.
                }
                try {
                    if (on_done) {
                        on_done();
                    }
                } catch (...) {
                }
                self->on_finished(self, id);
            });

        if (!ok) {
            std::lock_guard lock(mutex_);
            if (id < nodes_.size()) {
                nodes_[id].submitted = false;
            }
        }
        return ok;
    }

    void on_finished(const std::shared_ptr<TaskGraphState>& self, std::uint64_t id) {
        std::vector<std::uint64_t> ready;
        std::function<void()> complete;
        {
            std::lock_guard lock(mutex_);
            if (id < nodes_.size()) {
                for (std::uint64_t succ : nodes_[id].successors) {
                    if (succ < nodes_.size()) {
                        auto& s = nodes_[succ];
                        if (s.deps_remaining > 0) {
                            --s.deps_remaining;
                        }
                        if (s.deps_remaining == 0 && !s.submitted) {
                            ready.push_back(succ);
                        }
                    }
                }
            }
            if (remaining_ > 0) {
                --remaining_;
            }
            if (remaining_ == 0) {
                complete = std::move(on_complete_);
                on_complete_ = nullptr;
            }
        }

        for (std::uint64_t succ : ready) {
            (void)submit_node(self, succ);
        }

        if (complete) {
            try {
                complete();
            } catch (...) {
            }
        }

        {
            std::lock_guard lock(mutex_);
            if (remaining_ == 0) {
                done_cv_.notify_all();
            }
        }
    }

    ThreadPool& pool_;
    mutable std::mutex mutex_;
    std::condition_variable done_cv_;
    std::vector<Node> nodes_;
    std::size_t remaining_{0};
    bool started_{false};
    bool stopping_{false};
    std::function<void()> on_complete_;
};

}  // namespace detail

/// Opaque task id for TaskGraph::precede / add.
class TaskId {
public:
    TaskId() = default;

    [[nodiscard]] explicit operator bool() const noexcept { return valid_; }

private:
    friend class TaskGraph;

    explicit TaskId(std::uint64_t id) : id_(id), valid_(true) {}

    std::uint64_t id_{0};
    bool valid_{false};
};

/// Lightweight task DAG executed through a ThreadPool.
///
/// Design notes:
/// - Build the graph with add / precede, then try_run() once.
/// - A task runs only after every predecessor has finished (work + optional
///   on_done). Successors are not unlocked until on_done returns.
/// - try_run / dependent launches use ThreadPool::try_submit (never block the
///   caller on work). Returns false if any submit fails (pool full/stopped).
/// - Optional per-task on_done runs on a pool worker after that task's work
///   (same spirit as ThreadPool::try_submit). Exceptions are swallowed.
/// - Optional on_complete runs when the last task finishes (pool worker thread,
///   or the caller thread for an empty graph).
/// - wait() is for tests / app join points; prefer on_complete in async code.
/// - Cycles are not detected; a cycle leaves dependent tasks stranded.
/// - ThreadPool is non-owning and must outlive the graph's running tasks.
class TaskGraph {
public:
    explicit TaskGraph(ThreadPool& pool)
        : state_(std::make_shared<detail::TaskGraphState>(pool)) {}

    TaskGraph(const TaskGraph&) = delete;
    TaskGraph& operator=(const TaskGraph&) = delete;
    TaskGraph(TaskGraph&&) = delete;
    TaskGraph& operator=(TaskGraph&&) = delete;

    ~TaskGraph() { state_->request_stop(); }

    [[nodiscard]] TaskId add(std::function<void()> work) {
        return add(std::move(work), nullptr);
    }

    /// on_done (optional) runs on a worker after work, before successors start.
    [[nodiscard]] TaskId add(std::function<void()> work, std::function<void()> on_done) {
        return TaskId(state_->add(std::move(work), std::move(on_done)));
    }

    /// Ensure `before` completes before `after` starts.
    void precede(TaskId before, TaskId after) {
        if (!before || !after) {
            throw std::invalid_argument("TaskGraph::precede: invalid TaskId");
        }
        state_->precede(before.id_, after.id_);
    }

    void set_on_complete(std::function<void()> cb) {
        state_->set_on_complete(std::move(cb));
    }

    /// Kick off all currently ready tasks (zero predecessors).
    [[nodiscard]] bool try_run() { return state_->try_run(state_); }

    void wait() { state_->wait(); }

private:
    std::shared_ptr<detail::TaskGraphState> state_;
};

}  // namespace portfolio
