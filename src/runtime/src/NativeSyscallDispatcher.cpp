#include <ps5emu/runtime/NativeSyscallDispatcher.hpp>

#include <limits>
#include <stdexcept>

namespace ps5emu::runtime {
namespace {

constexpr std::uint64_t kSysGetpid = 20;
constexpr std::uint64_t kSysGetuid = 24;
constexpr std::uint64_t kSysGeteuid = 25;
constexpr std::uint64_t kCarryFlag = 1;

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

} // namespace

NativeSyscallDispatcher::NativeSyscallDispatcher(
    GuestProcessIdentity identity) noexcept
    : identity_(identity) {
}

NativeSyscallDispatchResult
NativeSyscallDispatcher::Dispatch(
    SysvGuestContext& context,
    std::uint64_t syscallAddress) const {
    const auto number = context.rax;

    switch (number) {
    case kSysGetpid:
        CompleteSuccess(
            context,
            syscallAddress,
            identity_.processId);
        return {
            .handled = true,
            .syscallNumber = number,
        };

    case kSysGetuid:
        CompleteSuccess(
            context,
            syscallAddress,
            identity_.userId);
        return {
            .handled = true,
            .syscallNumber = number,
        };

    case kSysGeteuid:
        CompleteSuccess(
            context,
            syscallAddress,
            identity_.effectiveUserId);
        return {
            .handled = true,
            .syscallNumber = number,
        };

    default:
        return {
            .handled = false,
            .syscallNumber = number,
        };
    }
}

} // namespace ps5emu::runtime
