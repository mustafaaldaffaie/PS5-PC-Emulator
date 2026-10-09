#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>

namespace ps5emu::logging {

enum class Level : std::uint8_t {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Critical = 5
};

struct Record {
    std::uint64_t sequence = 0;
    std::chrono::system_clock::time_point timestamp;
    Level level = Level::Info;
    std::string category;
    std::string message;
};

using Sink = std::function<void(const Record&)>;

class Logger final {
public:
    Logger();
    explicit Logger(Sink sink);

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    static Logger& Default();

    void SetMinimumLevel(Level level) noexcept;
    [[nodiscard]] Level MinimumLevel() const noexcept;

    void SetSink(Sink sink);

    void Write(Level level,
               std::string_view category,
               std::string_view message);

private:
    [[nodiscard]] Sink SnapshotSink() const;

    mutable std::mutex mutex_;
    Sink sink_;
    std::atomic<std::uint64_t> sequence_{0};
    std::atomic<Level> minimumLevel_{Level::Info};
};

[[nodiscard]] std::string_view
LevelName(Level level) noexcept;

} // namespace ps5emu::logging
