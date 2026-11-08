#pragma once
// spdlog has no websocket sink. This is the whole bridge: a sink pushes each
// formatted line into a queue, one log worker drains it to a file and a ring
// buffer, and the websocket replays that buffer. ponytail: fixed 200-line
// replay; raise it or page from the file when the log page needs history.
#include <readerwriterqueue.h>

#include <spdlog/sinks/base_sink.h>

#include <deque>
#include <fstream>
#include <mutex>
#include <string>
namespace {
inline moodycamel::ReaderWriterQueue<std::string> g_log_queue{1024};
}

class QueueSink : public spdlog::sinks::base_sink<std::mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t buf;
        formatter_->format(msg, buf);
        g_log_queue.enqueue(fmt::to_string(buf));
    }
    void flush_() override {}
};

class LogBuffer {
public:
    explicit LogBuffer(const std::string& path) : file_(path, std::ios::app) {}

    void drain() {
        std::string line;
        while (g_log_queue.try_dequeue(line))
            push(std::move(line));
    }

    std::vector<std::string> snapshot() const {
        std::scoped_lock lock(mu_);
        return {lines_.begin(), lines_.end()};
    }

private:
    void push(std::string line) {
        file_ << line << std::flush;
        std::scoped_lock lock(mu_);
        lines_.push_back(std::move(line));
        if (lines_.size() > 200)
            lines_.pop_front();
    }

    std::ofstream file_;
    mutable std::mutex mu_;
    std::deque<std::string> lines_;
};
