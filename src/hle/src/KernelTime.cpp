#include <ps5emu/hle/KernelTime.hpp>

#include <chrono>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

namespace ps5emu::hle {
namespace {

using Clock = std::chrono::steady_clock;

struct TimeState {
    Clock::time_point start = Clock::now();
};

std::uint64_t ElapsedNanoseconds(
    const TimeState& state) {
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - state.start);

    if (elapsed.count() < 0) {
        return 0;
    }

    return static_cast<std::uint64_t>(elapsed.count());
}

void GetProcessTime(
    HleCallFrame& frame,
    const TimeState& state) {
    frame.returnValue =
        ElapsedNanoseconds(state) / 1000u;
}

void GetProcessTimeCounter(
    HleCallFrame& frame,
    const TimeState& state) {
    frame.returnValue =
        ElapsedNanoseconds(state);
}

void GetProcessTimeCounterFrequency(
    HleCallFrame& frame) {
    frame.returnValue = 1000000000ull;
}

void SleepMicroseconds(HleCallFrame& frame) {
    const auto microseconds = frame.arguments[0];

    if (microseconds >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error(
            "Guest microsecond sleep duration is too large");
    }

    std::this_thread::sleep_for(
        std::chrono::microseconds(
            static_cast<std::int64_t>(microseconds)));

    frame.returnValue = 0;
}

void SleepSeconds(HleCallFrame& frame) {
    const auto seconds = frame.arguments[0];

    if (seconds >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error(
            "Guest second sleep duration is too large");
    }

    std::this_thread::sleep_for(
        std::chrono::seconds(
            static_cast<std::int64_t>(seconds)));

    frame.returnValue = 0;
}

} // namespace

void KernelTime::Register(
    HleRegistry& registry,
    std::string module) {
    if (module.empty()) {
        throw std::invalid_argument(
            "Kernel time module name cannot be empty");
    }

    auto state = std::make_shared<TimeState>();
    const auto moduleName = module;

    registry.RegisterSymbol(
        moduleName,
        "sceKernelGetProcessTime",
        [state](HleCallFrame& frame) {
            GetProcessTime(frame, *state);
        });

    registry.RegisterSymbol(
        moduleName,
        "sceKernelGetProcessTimeCounter",
        [state](HleCallFrame& frame) {
            GetProcessTimeCounter(frame, *state);
        });

    registry.RegisterSymbol(
        moduleName,
        "sceKernelGetProcessTimeCounterFrequency",
        GetProcessTimeCounterFrequency);

    registry.RegisterSymbol(
        moduleName,
        "sceKernelUsleep",
        SleepMicroseconds);

    registry.RegisterSymbol(
        std::move(module),
        "sceKernelSleep",
        SleepSeconds);
}

} // namespace ps5emu::hle
