// Copyright (c) 2026 Liam Mercier
//
// This file is part of Loadshear.
//
// Loadshear is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License Version 3.0
// as published by the Free Software Foundation.
//
// Loadshear is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License v3.0
// for more details.
//
// You should have received a copy of the GNU General Public License v3.0
// along with Loadshear. If not, see <https://www.gnu.org/licenses/gpl-3.0.txt>

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

    a[static_cast<size_t>(LogLevel::DEBUG)] = "\033[36m[DEBUG]:\033[0m ";
    a[static_cast<size_t>(LogLevel::INFO)] = "";
    a[static_cast<size_t>(LogLevel::WARN)] = "\033[32m[WARN]:\033[0m ";
    a[static_cast<size_t>(LogLevel::ERROR)] = "\033[31m[ERROR]:\033[0m ";

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

    static void set_level(LogLevel level)
    {
        instance().level_ = level;
    }

    static bool should_log(LogLevel level)
    {
        return level >= instance().level_;
    }

    static void log(LogLevel level, std::string msg)
    {
        instance().push_message(level, std::move(msg));
    }

    // Helpers to make logging easier.
    static void debug(std::string msg)
    {
        instance().push_message(LogLevel::DEBUG, std::move(msg));
    }

    static void info(std::string msg)
    {
        instance().push_message(LogLevel::INFO, std::move(msg));
    }

    static void warn(std::string msg)
    {
        instance().push_message(LogLevel::WARN, std::move(msg));
    }

    static void error(std::string msg)
    {
        instance().push_message(LogLevel::ERROR, std::move(msg));
    }

    // Control printing during TUI.
    static void pause()
    {
        {
            std::lock_guard lock(instance().queue_mutex_);
            instance().notify_ = false;
        }
    }

    static void resume()
    {
        {
            std::lock_guard lock(instance().queue_mutex_);
            instance().notify_ = true;
        }

        instance().cv_.notify_one();
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
    bool notify_{true};

    // We don't expect much queue contention since it is infrequent that
    // we will log from multiple shards at once.
    std::mutex queue_mutex_;
    std::vector<LogEntry> msg_queue_;

    // Prevent thread work when we are not busy.
    std::condition_variable cv_;

    std::jthread worker_;
};
