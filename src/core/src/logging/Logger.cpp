#include <ps5emu/logging/Logger.hpp>

#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace ps5emu::logging {
namespace {

void ConsoleSink(const Record& record) {
    static std::mutex consoleMutex;
    std::lock_guard lock(consoleMutex);

    const auto time =
        std::chrono::system_clock::to_time_t(
            record.timestamp);

    std::tm local{};

#if defined(_WIN32)
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif

    std::clog
        << '['
        << std::put_time(&local, "%Y-%m-%d %H:%M:%S")
        << "] ["
        << LevelName(record.level)
        << "] ["
        << record.category
        << "] "
        << record.message
        << '\n';
}

} // namespace

Logger::Logger()
    : sink_(ConsoleSink) {
}

Logger::Logger(Sink sink)
    : sink_(std::move(sink)) {
    if (!sink_) {
        throw std::invalid_argument(
            "Logger sink cannot be empty");
    }
}

Logger& Logger::Default() {
    static Logger logger;
    return logger;
}

void Logger::SetMinimumLevel(Level level) noexcept {
    minimumLevel_.store(
        level,
        std::memory_order_relaxed);
}

Level Logger::MinimumLevel() const noexcept {
    return minimumLevel_.load(
        std::memory_order_relaxed);
}

void Logger::SetSink(Sink sink) {
    if (!sink) {
        throw std::invalid_argument(
            "Logger sink cannot be empty");
    }

    std::lock_guard lock(mutex_);
    sink_ = std::move(sink);
}

void Logger::Write(Level level,
                   std::string_view category,
                   std::string_view message) {
    if (static_cast<std::uint8_t>(level) <
        static_cast<std::uint8_t>(
            MinimumLevel())) {
        return;
    }

    auto sink = SnapshotSink();

    Record record;
    record.sequence =
        sequence_.fetch_add(
            1,
            std::memory_order_relaxed);
    record.timestamp =
        std::chrono::system_clock::now();
    record.level = level;
    record.category = category;
    record.message = message;

    sink(record);
}

Sink Logger::SnapshotSink() const {
    std::lock_guard lock(mutex_);
    return sink_;
}

std::string_view
LevelName(Level level) noexcept {
    switch (level) {
    case Level::Trace:
        return "trace";
    case Level::Debug:
        return "debug";
    case Level::Info:
        return "info";
    case Level::Warning:
        return "warning";
    case Level::Error:
        return "error";
    case Level::Critical:
        return "critical";
    }

    return "unknown";
}

} // namespace ps5emu::logging
