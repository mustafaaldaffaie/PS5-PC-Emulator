#include <ps5emu/hle/KernelTime.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

namespace ps5emu::hle {
namespace {

using Clock = std::chrono::steady_clock;

constexpr std::uint64_t kSceKernelErrorFault =
    0x8002000eull;
constexpr std::uint64_t kSceKernelErrorInvalid =
    0x80020016ull;

struct GuestTimespec {
    std::int64_t seconds = 0;
    std::int64_t nanoseconds = 0;
};

struct GuestTimeval {
    std::int64_t seconds = 0;
    std::int64_t microseconds = 0;
};

static_assert(sizeof(GuestTimespec) == 16);
static_assert(sizeof(GuestTimeval) == 16);

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

    return static_cast<std::uint64_t>(
        elapsed.count());
}

GuestMemoryAccess* Memory(
    HleCallFrame& frame) noexcept {
    return frame.memory;
}

template <typename T>
bool ReadGuest(
    HleCallFrame& frame,
    std::uint64_t address,
    T& value) {
    if (address == 0 ||
        Memory(frame) == nullptr) {
        return false;
    }

    std::array<std::byte, sizeof(T)> bytes{};

    try {
        Memory(frame)->Read(
            address,
            bytes);
    } catch (const std::exception&) {
        return false;
    }

    std::memcpy(
        &value,
        bytes.data(),
        sizeof(value));
    return true;
}

template <typename T>
bool WriteGuest(
    HleCallFrame& frame,
    std::uint64_t address,
    const T& value) {
    if (address == 0 ||
        Memory(frame) == nullptr) {
        return false;
    }

    std::array<std::byte, sizeof(T)> bytes{};
    std::memcpy(
        bytes.data(),
        &value,
        sizeof(value));

    try {
        Memory(frame)->Write(
            address,
            bytes);
    } catch (const std::exception&) {
        return false;
    }

    return true;
}

std::uint64_t RealtimeNanoseconds() {
    const auto value =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now()
                .time_since_epoch());

    if (value.count() < 0) {
        throw std::runtime_error(
            "Host realtime clock precedes the Unix epoch");
    }

    return static_cast<std::uint64_t>(
        value.count());
}

std::uint64_t MonotonicNanoseconds() {
    const auto value =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now().time_since_epoch());

    if (value.count() < 0) {
        return 0;
    }

    return static_cast<std::uint64_t>(
        value.count());
}

std::uint64_t ProcessCpuNanoseconds() {
    const auto ticks = std::clock();

    if (ticks == static_cast<std::clock_t>(-1)) {
        throw std::runtime_error(
            "Host process CPU clock is unavailable");
    }

    const auto value =
        static_cast<long double>(ticks) *
        1000000000.0L /
        static_cast<long double>(CLOCKS_PER_SEC);

    if (value < 0.0L ||
        value >
            static_cast<long double>(
                std::numeric_limits<std::uint64_t>::max())) {
        throw std::overflow_error(
            "Host process CPU clock overflows guest time");
    }

    return static_cast<std::uint64_t>(value);
}

GuestTimespec SplitNanoseconds(
    std::uint64_t nanoseconds) {
    return GuestTimespec{
        .seconds =
            static_cast<std::int64_t>(
                nanoseconds / 1000000000ull),
        .nanoseconds =
            static_cast<std::int64_t>(
                nanoseconds % 1000000000ull),
    };
}

bool ResolveClock(
    std::int32_t clockId,
    GuestTimespec& value) {
    switch (clockId) {
    case 0:
    case 9:
    case 10:
    case 13:
        value =
            SplitNanoseconds(
                RealtimeNanoseconds());
        if (clockId == 13) {
            value.nanoseconds = 0;
        }
        return true;

    case 1:
    case 2:
        value =
            SplitNanoseconds(
                ProcessCpuNanoseconds());
        return true;

    case 4:
    case 5:
    case 7:
    case 8:
    case 11:
    case 12:
        value =
            SplitNanoseconds(
                MonotonicNanoseconds());
        return true;

    default:
        return false;
    }
}

template <typename HostClock>
std::uint64_t ClockResolutionNanoseconds() {
    using Period = typename HostClock::period;

    const auto numerator =
        static_cast<unsigned long long>(
            Period::num);
    const auto denominator =
        static_cast<unsigned long long>(
            Period::den);

    constexpr std::uint64_t billion =
        1000000000ull;

    if (numerator >
        std::numeric_limits<std::uint64_t>::max() /
            billion) {
        return 1;
    }

    const auto scaled =
        numerator * billion;
    const auto rounded =
        (scaled + denominator - 1) /
        denominator;

    return rounded == 0 ? 1 : rounded;
}

bool ResolveClockResolution(
    std::int32_t clockId,
    GuestTimespec& value) {
    std::uint64_t nanoseconds = 0;

    switch (clockId) {
    case 0:
    case 9:
    case 10:
    case 13:
        nanoseconds =
            ClockResolutionNanoseconds<
                std::chrono::system_clock>();
        break;

    case 1:
    case 2:
        nanoseconds =
            (1000000000ull +
             static_cast<std::uint64_t>(
                 CLOCKS_PER_SEC) -
             1) /
            static_cast<std::uint64_t>(
                CLOCKS_PER_SEC);
        break;

    case 4:
    case 5:
    case 7:
    case 8:
    case 11:
    case 12:
        nanoseconds =
            ClockResolutionNanoseconds<Clock>();
        break;

    default:
        return false;
    }

    value =
        SplitNanoseconds(
            nanoseconds);
    return true;
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

void ClockGettime(HleCallFrame& frame) {
    const auto clockId =
        static_cast<std::int32_t>(
            frame.arguments[0]);
    const auto outputAddress =
        frame.arguments[1];

    if (outputAddress == 0 ||
        Memory(frame) == nullptr) {
        frame.returnValue =
            kSceKernelErrorFault;
        return;
    }

    GuestTimespec value;
    if (!ResolveClock(
            clockId,
            value)) {
        frame.returnValue =
            kSceKernelErrorInvalid;
        return;
    }

    if (!WriteGuest(
            frame,
            outputAddress,
            value)) {
        frame.returnValue =
            kSceKernelErrorFault;
        return;
    }

    frame.returnValue = 0;
}

void ClockGetres(HleCallFrame& frame) {
    const auto clockId =
        static_cast<std::int32_t>(
            frame.arguments[0]);
    const auto outputAddress =
        frame.arguments[1];

    if (outputAddress == 0 ||
        Memory(frame) == nullptr) {
        frame.returnValue =
            kSceKernelErrorFault;
        return;
    }

    GuestTimespec value;
    if (!ResolveClockResolution(
            clockId,
            value)) {
        frame.returnValue =
            kSceKernelErrorInvalid;
        return;
    }

    if (!WriteGuest(
            frame,
            outputAddress,
            value)) {
        frame.returnValue =
            kSceKernelErrorFault;
        return;
    }

    frame.returnValue = 0;
}

void Gettimeofday(HleCallFrame& frame) {
    const auto outputAddress =
        frame.arguments[0];

    if (outputAddress == 0 ||
        Memory(frame) == nullptr) {
        frame.returnValue =
            kSceKernelErrorFault;
        return;
    }

    const auto microseconds =
        RealtimeNanoseconds() / 1000ull;

    const GuestTimeval value{
        .seconds =
            static_cast<std::int64_t>(
                microseconds / 1000000ull),
        .microseconds =
            static_cast<std::int64_t>(
                microseconds % 1000000ull),
    };

    if (!WriteGuest(
            frame,
            outputAddress,
            value)) {
        frame.returnValue =
            kSceKernelErrorFault;
        return;
    }

    frame.returnValue = 0;
}

void Nanosleep(HleCallFrame& frame) {
    const auto requestAddress =
        frame.arguments[0];
    const auto remainingAddress =
        frame.arguments[1];

    GuestTimespec request;
    if (!ReadGuest(
            frame,
            requestAddress,
            request)) {
        frame.returnValue =
            kSceKernelErrorFault;
        return;
    }

    if (request.seconds < 0 ||
        request.nanoseconds < 0 ||
        request.nanoseconds >= 1000000000ll) {
        frame.returnValue =
            kSceKernelErrorInvalid;
        return;
    }

    const auto seconds =
        static_cast<std::uint64_t>(
            request.seconds);
    const auto nanoseconds =
        static_cast<std::uint64_t>(
            request.nanoseconds);

    if (seconds >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max() /
            1000000000ll)) {
        throw std::overflow_error(
            "Guest nanosleep duration is too large");
    }

    const auto totalNanoseconds =
        seconds * 1000000000ull +
        nanoseconds;

    if (totalNanoseconds >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error(
            "Guest nanosleep duration is too large");
    }

    std::this_thread::sleep_for(
        std::chrono::nanoseconds(
            static_cast<std::int64_t>(
                totalNanoseconds)));

    if (remainingAddress != 0) {
        const GuestTimespec remaining{};

        if (!WriteGuest(
                frame,
                remainingAddress,
                remaining)) {
            frame.returnValue =
                kSceKernelErrorFault;
            return;
        }
    }

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
        moduleName,
        "sceKernelSleep",
        SleepSeconds);

    registry.RegisterSymbol(
        moduleName,
        "sceKernelClockGettime",
        ClockGettime);

    registry.RegisterSymbol(
        moduleName,
        "sceKernelClockGetres",
        ClockGetres);

    registry.RegisterSymbol(
        moduleName,
        "sceKernelGettimeofday",
        Gettimeofday);

    registry.RegisterSymbol(
        std::move(module),
        "sceKernelNanosleep",
        Nanosleep);
}

} // namespace ps5emu::hle
