#pragma once

#include "spsc_ring_buffer/spsc_ring_buffer.hpp"

#include <condition_variable>
#include <cstddef>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace portfolio {

enum class LogLevel { debug, info, warn, error };

namespace detail {

struct LogRecord {
    LogLevel level{LogLevel::info};
    std::string message;
};

[[nodiscard]] inline const char* level_tag(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::debug:
            return "[DEBUG] ";
        case LogLevel::info:
            return "[INFO] ";
        case LogLevel::warn:
            return "[WARN] ";
        case LogLevel::error:
            return "[ERROR] ";
    }
    return "[INFO] ";
}

}  // namespace detail

/// Async logger: callers enqueue records; a dedicated thread writes to a sink.
///
/// Design notes:
/// - try_log never blocks on I/O; returns false if the queue is full or shutdown.
/// - Queue is `02` SpscRingBuffer. A producer mutex serializes try_push so many
///   app threads may call try_log while the queue still sees one producer.
/// - One consumer thread owns try_pop and the sink (SPSC consumer role).
/// - shutdown() stops accepts, drains remaining records, then joins the writer.
/// - Destructor calls shutdown() if not already shut down.
/// - Prefer console_sink() / file_sink(path) helpers, or pass any custom Sink.
class AsyncLogger {
public:
    using Sink = std::function<void(LogLevel, std::string_view)>;

    static constexpr std::size_t kDefaultCapacity = 1024;

    /// Default console sink: warn/error → stderr, otherwise → stdout.
    [[nodiscard]] static Sink console_sink() {
        return [](LogLevel level, std::string_view message) {
            std::ostream& out =
                (level == LogLevel::error || level == LogLevel::warn) ? std::cerr : std::cout;
            out << detail::level_tag(level) << message << '\n';
        };
    }

    /// File sink that keeps one ofstream open for the sink's lifetime.
    /// Throws std::runtime_error if the path cannot be opened for append.
    [[nodiscard]] static Sink file_sink(std::string path) {
        auto file = std::make_shared<std::ofstream>(path, std::ios::app);
        if (!file->is_open()) {
            throw std::runtime_error("AsyncLogger::file_sink failed to open: " + path);
        }
        return [file](LogLevel level, std::string_view message) {
            *file << detail::level_tag(level) << message << '\n';
        };
    }

    explicit AsyncLogger(std::size_t capacity = kDefaultCapacity, Sink sink = {})
        : queue_(capacity), sink_(sink ? std::move(sink) : console_sink()) {
        if (capacity == 0) {
            throw std::invalid_argument("AsyncLogger capacity must be > 0");
        }
        worker_ = std::thread([this] { consumer_loop(); });
    }

    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;
    AsyncLogger(AsyncLogger&&) = delete;
    AsyncLogger& operator=(AsyncLogger&&) = delete;

    ~AsyncLogger() { shutdown(); }

    /// Non-blocking. Copies `message`, enqueues, wakes the writer.
    [[nodiscard]] bool try_log(LogLevel level, std::string_view message) {
        detail::LogRecord record{level, std::string(message)};
        {
            std::lock_guard lock(mutex_);
            if (stopping_) {
                return false;
            }
            if (!queue_.try_push(std::move(record))) {
                return false;
            }
        }
        cv_.notify_one();
        return true;
    }

    /// Stop accepting logs, drain the queue through the sink, join the writer.
    void shutdown() {
        {
            std::lock_guard lock(mutex_);
            if (stopping_) {
                // Already shutting down / shut down — still join if needed.
            } else {
                stopping_ = true;
            }
        }
        cv_.notify_all();

        if (worker_.joinable()) {
            worker_.join();
        }
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return queue_.capacity(); }

private:
    void consumer_loop() {
        while (true) {
            while (auto item = queue_.try_pop()) {
                try {
                    sink_(item->level, item->message);
                } catch (...) {
                    // Keep the writer alive; sink errors must not kill logging.
                }
            }

            {
                std::unique_lock lock(mutex_);
                if (stopping_ && queue_.empty()) {
                    return;
                }
                cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            }
        }
    }

    SpscRingBuffer<detail::LogRecord> queue_;
    Sink sink_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_{false};
    std::thread worker_;
};

}  // namespace portfolio
