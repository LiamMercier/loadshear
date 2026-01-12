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

#include "logger.h"

#include <iostream>

void Logger::init(LogLevel init_level)
{
    Logger & inst = instance();
    inst.level_ = init_level;
    inst.running_ = true;

    inst.worker_ = std::jthread([&inst](){
        inst.worker_loop();
    });
}

void Logger::shutdown()
{
    auto & inst = instance();

    {
        // prevent more messages during shutdown since we wait on a cv here.
        std::lock_guard lock(inst.queue_mutex_);
        inst.running_ = false;
    }

    inst.cv_.notify_one();

    // Our thread is a jthread and will auto join.
}

Logger::~Logger()
{
    if (running_)
    {
        shutdown();
    }
}

Logger & Logger::instance()
{
    static Logger instance;
    return instance;
}

// Already in the instance, no need to grab again.
void Logger::push_message(LogLevel level, std::string msg)
{
    bool should_notify = false;
    {
        std::lock_guard lock(queue_mutex_);
        msg_queue_.emplace_back(LogEntry{level, std::move(msg)});
        should_notify = notify_;
    }

    if (should_notify)
    {
        // Do work if waiting (wakes up thread).
        cv_.notify_one();
    }
}

void Logger::worker_loop()
{
    // This thread runs the entire time, reserve our own messages queue
    // using some preallocation to prevent allocations during run.
    std::vector<LogEntry> processing_queue;
    processing_queue.reserve(PREALLOCATE_QUEUE_SIZE);

    while (true)
    {
        // Lock the queue and wait.
        {
            std::unique_lock lock(queue_mutex_);

            cv_.wait(lock, [this] {
                return (notify_ && !msg_queue_.empty()) || !running_;
            });

            // When we wake up, swap the queues so we own all of the
            // messages that have been pushed so far.
            std::swap(msg_queue_, processing_queue);

            // Prevent doing work if we stopped and have no more messages.
            if (!running_ && processing_queue.empty())
            {
                return;
            }
        }

        // Drop the lock, process the messages now.
        for (const auto & entry : processing_queue)
        {
            if (entry.level < level_)
            {
                continue;
            }

            std::cout << log_prefix[static_cast<size_t>(entry.level)]
                      << entry.msg
                      << "\n";
        }

        // Clear and restart the loop (thread goes back to sleep).
        processing_queue.clear();
    }
}
