#include "async_logger/async_logger.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace {

struct CapturingSink {
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::pair<klib::LogLevel, std::string>> records;
    std::atomic<bool> block_writes{false};
    std::mutex block_mutex;
    std::condition_variable block_cv;
    bool allow_write{true};

    void operator()(klib::LogLevel level, std::string_view message) {
        if (block_writes.load(std::memory_order_acquire)) {
            std::unique_lock lock(block_mutex);
            block_cv.wait(lock, [this] { return allow_write; });
        }

        {
            std::lock_guard lock(mutex);
            records.emplace_back(level, std::string(message));
        }
        cv.notify_all();
    }

    void wait_for_count(std::size_t n) {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(2),
                                [&] { return records.size() >= n; }));
    }

    void unblock() {
        {
            std::lock_guard lock(block_mutex);
            allow_write = true;
            block_writes.store(false, std::memory_order_release);
        }
        block_cv.notify_all();
    }
};

}  // namespace

TEST(AsyncLogger, TryLogDeliversThroughSink) {
    auto sink = std::make_shared<CapturingSink>();
    klib::AsyncLogger logger(8, [sink](klib::LogLevel level,
                                            std::string_view msg) { (*sink)(level, msg); });

    ASSERT_TRUE(logger.try_log(klib::LogLevel::info, "hello"));
    sink->wait_for_count(1);
    logger.shutdown();

    ASSERT_EQ(sink->records.size(), 1u);
    EXPECT_EQ(sink->records[0].first, klib::LogLevel::info);
    EXPECT_EQ(sink->records[0].second, "hello");
}

TEST(AsyncLogger, ShutdownDrainsQueuedRecords) {
    auto sink = std::make_shared<CapturingSink>();
    klib::AsyncLogger logger(16, [sink](klib::LogLevel level,
                                             std::string_view msg) { (*sink)(level, msg); });

    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(logger.try_log(klib::LogLevel::debug, std::to_string(i)));
    }
    logger.shutdown();

    ASSERT_EQ(sink->records.size(), 10u);
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(sink->records[static_cast<std::size_t>(i)].second, std::to_string(i));
    }
}

TEST(AsyncLogger, TryLogFailsAfterShutdown) {
    auto sink = std::make_shared<CapturingSink>();
    klib::AsyncLogger logger(8, [sink](klib::LogLevel level,
                                            std::string_view msg) { (*sink)(level, msg); });

    ASSERT_TRUE(logger.try_log(klib::LogLevel::warn, "before"));
    logger.shutdown();
    EXPECT_FALSE(logger.try_log(klib::LogLevel::warn, "after"));
}

TEST(AsyncLogger, TryLogFailsWhenQueueFull) {
    auto sink = std::make_shared<CapturingSink>();
    sink->block_writes.store(true, std::memory_order_release);
    sink->allow_write = false;

    // Capacity rounds up to power of two in SPSC (4 stays 4).
    klib::AsyncLogger logger(4, [sink](klib::LogLevel level,
                                            std::string_view msg) { (*sink)(level, msg); });

    // First record may be held by the consumer waiting on the blocked sink.
    // Fill remaining slots until try_log reports full.
    std::size_t accepted = 0;
    for (int i = 0; i < 32; ++i) {
        if (logger.try_log(klib::LogLevel::error, "x")) {
            ++accepted;
        } else {
            break;
        }
    }
    EXPECT_GE(accepted, 1u);
    EXPECT_LE(accepted, logger.capacity() + 1);  // +1 if one is mid-sink
    EXPECT_FALSE(logger.try_log(klib::LogLevel::error, "overflow"));

    sink->unblock();
    logger.shutdown();
    EXPECT_EQ(sink->records.size(), accepted);
}

TEST(AsyncLogger, PreservesOrderFromSingleProducer) {
    auto sink = std::make_shared<CapturingSink>();
    klib::AsyncLogger logger(64, [sink](klib::LogLevel level,
                                             std::string_view msg) { (*sink)(level, msg); });

    constexpr int kCount = 50;
    for (int i = 0; i < kCount; ++i) {
        ASSERT_TRUE(logger.try_log(klib::LogLevel::info, std::to_string(i)));
    }
    logger.shutdown();

    ASSERT_EQ(sink->records.size(), static_cast<std::size_t>(kCount));
    for (int i = 0; i < kCount; ++i) {
        EXPECT_EQ(sink->records[static_cast<std::size_t>(i)].second, std::to_string(i));
    }
}

TEST(AsyncLogger, ConcurrentProducersDoNotCrash) {
    auto sink = std::make_shared<CapturingSink>();
    klib::AsyncLogger logger(256, [sink](klib::LogLevel level,
                                              std::string_view msg) { (*sink)(level, msg); });

    constexpr int kThreads = 4;
    constexpr int kPerThread = 40;
    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    std::atomic<int> accepted{0};

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&logger, &accepted, t] {
            for (int i = 0; i < kPerThread; ++i) {
                if (logger.try_log(klib::LogLevel::info,
                                   "t" + std::to_string(t) + "-" + std::to_string(i))) {
                    accepted.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    logger.shutdown();

    EXPECT_EQ(static_cast<int>(sink->records.size()), accepted.load());
    EXPECT_EQ(accepted.load(), kThreads * kPerThread);
}

TEST(AsyncLogger, ZeroCapacityThrows) {
    EXPECT_THROW(klib::AsyncLogger(0), std::invalid_argument);
}

TEST(AsyncLogger, FileSinkWritesAndKeepsHandleOpen) {
    const std::string path = "async_logger_file_sink_test.log";
    std::remove(path.c_str());

    {
        klib::AsyncLogger logger(8, klib::AsyncLogger::file_sink(path));
        ASSERT_TRUE(logger.try_log(klib::LogLevel::info, "line-one"));
        ASSERT_TRUE(logger.try_log(klib::LogLevel::error, "line-two"));
        logger.shutdown();
    }

    std::ifstream in(path);
    ASSERT_TRUE(in.is_open());
    std::ostringstream contents;
    contents << in.rdbuf();
    EXPECT_EQ(contents.str(), "[INFO] line-one\n[ERROR] line-two\n");

    std::remove(path.c_str());
}

TEST(AsyncLogger, FileSinkThrowsWhenPathCannotOpen) {
    EXPECT_THROW(klib::AsyncLogger::file_sink("/no/such/dir/async_logger.log"),
                 std::runtime_error);
}
