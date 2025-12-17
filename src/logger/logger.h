#pragma once

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

enum class LogLevel
{
    DEBUG,
    INFO,
    WARN,
    ERROR
};

constexpr size_t NUM_LOG_LEVELS = static_cast<size_t>(LogLevel::ERROR) + 1;

struct LogEntry
{
    LogLevel level;
    std::string msg;
};

// Set of prefix values to use when we log.
constexpr std::array<std::string_view, NUM_LOG_LEVELS> log_prefix = []
{
    std::array<std::string_view, NUM_LOG_LEVELS> a{};

    a[static_cast<size_t>(LogLevel::DEBUG)] = "[DEBUG]: ";
    a[static_cast<size_t>(LogLevel::INFO)] = "";
    a[static_cast<size_t>(LogLevel::WARN)] = "[WARN]: ";
    a[static_cast<size_t>(LogLevel::ERROR)] = "[ERROR]: ";

    return a;
}();

// Async singleton logger class.
class Logger
{
public:
    static constexpr size_t PREALLOCATE_QUEUE_SIZE = 100;

public:
    // Called once in main.
    static void init(LogLevel init_level);

    // Called at end of main.
    static void shutdown();

    static bool should_log(LogLevel level)
    {
        return level >= instance().level_;
    }

    static void log(LogLevel level, std::string msg)
    {
        instance().push_message(level, std::move(msg));
    }

private:
    Logger() = default;

    ~Logger();

    // Singleton.
    static Logger & instance();

    void push_message(LogLevel level, std::string msg);

    void worker_loop();

private:
    LogLevel level_{LogLevel::INFO};
    std::atomic<bool> running_{false};

    // We don't expect much queue contention since it is infrequent that
    // we will log from multiple shards at once.
    std::mutex queue_mutex_;
    std::vector<LogEntry> msg_queue_;

    // Prevent thread work when we are not busy.
    std::condition_variable cv_;

    std::jthread worker_;
};
