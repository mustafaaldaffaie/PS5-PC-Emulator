#include <ps5emu/runtime/NativeSyscallDispatcher.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <limits>
#include <stdexcept>

namespace ps5emu::runtime {
namespace {

constexpr std::uint64_t kSysGetpid = 20;
constexpr std::uint64_t kSysGetuid = 24;
constexpr std::uint64_t kSysGeteuid = 25;
constexpr std::uint64_t kSysGetppid = 39;
constexpr std::uint64_t kSysGetegid = 43;
constexpr std::uint64_t kSysGetgid = 47;
constexpr std::uint64_t kSysGettimeofday = 116;
constexpr std::uint64_t kSysClockGettime = 232;

constexpr std::uint64_t kErrFault = 14;
constexpr std::uint64_t kErrInvalid = 22;
constexpr std::uint64_t kCarryFlag = 1;

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

std::uint64_t CheckedNextRip(
    std::uint64_t syscallAddress) {
    if (syscallAddress >
        std::numeric_limits<std::uint64_t>::max() - 2) {
        throw std::overflow_error(
            "Guest syscall return address overflows");
    }

    return syscallAddress + 2;
}

void CompleteSuccess(
    SysvGuestContext& context,
    std::uint64_t syscallAddress,
    std::uint64_t value) {
    const auto nextRip =
        CheckedNextRip(syscallAddress);

    context.rax = value;
    context.rip = nextRip;
    context.rcx = nextRip;
    context.rflags &= ~kCarryFlag;
    context.r11 = context.rflags;
}

void CompleteError(
    SysvGuestContext& context,
    std::uint64_t syscallAddress,
    std::uint64_t error) {
    const auto nextRip =
        CheckedNextRip(syscallAddress);

    context.rax = error;
    context.rip = nextRip;
    context.rcx = nextRip;
    context.rflags |= kCarryFlag;
    context.r11 = context.rflags;
}

template <typename T>
bool WriteGuest(
    hle::GuestMemoryAccess* memory,
    std::uint64_t address,
    const T& value) {
    if (memory == nullptr || address == 0) {
        return false;
    }

    std::array<std::byte, sizeof(T)> bytes{};
    std::memcpy(
        bytes.data(),
        &value,
        sizeof(value));

    try {
        memory->Write(
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
            std::chrono::steady_clock::now()
                .time_since_epoch());

    return value.count() < 0
        ? 0
        : static_cast<std::uint64_t>(
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

    return static_cast<std::uint64_t>(
        value);
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

void DispatchGettimeofday(
    SysvGuestContext& context,
    std::uint64_t syscallAddress,
    hle::GuestMemoryAccess* memory) {
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
            memory,
            context.rdi,
            value)) {
        CompleteError(
            context,
            syscallAddress,
            kErrFault);
        return;
    }

    // The second gettimeofday argument is legacy timezone output. PS5 callers
    // and public implementations treat it as ignored.
    CompleteSuccess(
        context,
        syscallAddress,
        0);
}

void DispatchClockGettime(
    SysvGuestContext& context,
    std::uint64_t syscallAddress,
    hle::GuestMemoryAccess* memory) {
    GuestTimespec value;

    if (!ResolveClock(
            static_cast<std::int32_t>(
                context.rdi),
            value)) {
        CompleteError(
            context,
            syscallAddress,
            kErrInvalid);
        return;
    }

    if (!WriteGuest(
            memory,
            context.rsi,
            value)) {
        CompleteError(
            context,
            syscallAddress,
            kErrFault);
        return;
    }

    CompleteSuccess(
        context,
        syscallAddress,
        0);
}

} // namespace

NativeSyscallDispatcher::NativeSyscallDispatcher(
    GuestProcessIdentity identity) noexcept
    : identity_(identity) {
}

NativeSyscallDispatchResult
NativeSyscallDispatcher::Dispatch(
    SysvGuestContext& context,
    std::uint64_t syscallAddress,
    hle::GuestMemoryAccess* memory) const {
    const auto number = context.rax;

    switch (number) {
    case kSysGetpid:
        CompleteSuccess(
            context,
            syscallAddress,
            identity_.processId);
        break;

    case kSysGetuid:
        CompleteSuccess(
            context,
            syscallAddress,
            identity_.userId);
        break;

    case kSysGeteuid:
        CompleteSuccess(
            context,
            syscallAddress,
            identity_.effectiveUserId);
        break;

    case kSysGetppid:
        CompleteSuccess(
            context,
            syscallAddress,
            identity_.parentProcessId);
        break;

    case kSysGetegid:
        CompleteSuccess(
            context,
            syscallAddress,
            identity_.effectiveGroupId);
        break;

    case kSysGetgid:
        CompleteSuccess(
            context,
            syscallAddress,
            identity_.groupId);
        break;

    case kSysGettimeofday:
        DispatchGettimeofday(
            context,
            syscallAddress,
            memory);
        break;

    case kSysClockGettime:
        DispatchClockGettime(
            context,
            syscallAddress,
            memory);
        break;

    default:
        return {
            .handled = false,
            .syscallNumber = number,
        };
    }

    return {
        .handled = true,
        .syscallNumber = number,
    };
}

} // namespace ps5emu::runtime
