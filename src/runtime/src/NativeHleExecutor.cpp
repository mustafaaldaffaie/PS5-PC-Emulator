#include <ps5emu/runtime/NativeHleExecutor.hpp>

#include <algorithm>
#include <stdexcept>
#include <vector>

#include <ps5emu/runtime/HleTrapHandler.hpp>
#include <ps5emu/runtime/NativeGuestMemoryAccess.hpp>

namespace ps5emu::runtime {

NativeHleExecutionResult NativeHleExecutor::Run(
    SysvGuestContext& context,
    NativeImage& nativeImage,
    const hle::HleRegistry& registry,
    const HleThunkTable& thunks,
    std::span<const NativeSyscallTrap> syscallTraps,
    hle::GuestThreadAccess* threads,
    hle::GuestFileSystemAccess* files) {
    NativeHleExecutionResult result;

    if (thunks.Size() == 0 &&
        syscallTraps.empty()) {
        executor_.Run(
            context,
            nativeImage);
        return result;
    }

    std::vector<std::uint64_t> syscallAddresses;
    syscallAddresses.reserve(syscallTraps.size());
    for (const auto& trap : syscallTraps) {
        syscallAddresses.push_back(
            trap.guestAddress);
    }

    NativeGuestMemoryAccess nativeMemory(
        nativeImage);

    bool resume = false;

    while (true) {
        std::uint64_t capturedRip = 0;
        std::uint64_t capturedBreakpoint = 0;

        {
            auto scope =
                trapBridge_.Arm(
                    thunks.BaseAddress(),
                    thunks.SlotSize(),
                    thunks.Size(),
                    executor_.EscapeAddress(),
                    syscallAddresses);

            if (resume) {
                executor_.Resume(
                    context,
                    nativeImage);
            } else {
                executor_.Run(
                    context,
                    nativeImage);
            }

            capturedRip =
                scope.CapturedRip();
            capturedBreakpoint =
                scope.CapturedBreakpointAddress();
        }

        if (capturedRip == 0) {
            return result;
        }

        const auto syscall =
            std::find(
                syscallAddresses.begin(),
                syscallAddresses.end(),
                capturedBreakpoint);

        if (syscall != syscallAddresses.end()) {
            context.rip = capturedBreakpoint;

            const auto syscallResult =
                syscallDispatcher_.Dispatch(
                    context,
                    capturedBreakpoint,
                    &nativeMemory);

            result.syscallNumber =
                syscallResult.syscallNumber;
            result.syscallAddress =
                capturedBreakpoint;

            if (syscallResult.handled) {
                ++result.handledSyscallCount;
                resume = true;
                continue;
            }

            result.interceptedSyscall = true;
            return result;
        }

        context.rip = capturedRip;

        const auto trapResult =
            HleTrapHandler::HandleInt3(
                context,
                nativeMemory,
                registry,
                thunks,
                threads,
                files);

        if (!trapResult.handled) {
            throw std::runtime_error(
                "Captured native HLE trap could not be dispatched");
        }

        ++result.handledTrapCount;
        result.lastErrorCode =
            trapResult.errorCode;
        resume = true;
    }
}

} // namespace ps5emu::runtime
