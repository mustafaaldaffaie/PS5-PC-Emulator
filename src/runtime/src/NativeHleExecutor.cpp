#include <ps5emu/runtime/NativeHleExecutor.hpp>

#include <stdexcept>

#include <ps5emu/runtime/HleTrapHandler.hpp>
#include <ps5emu/runtime/NativeGuestMemoryAccess.hpp>

namespace ps5emu::runtime {

NativeHleExecutionResult NativeHleExecutor::Run(
    SysvGuestContext& context,
    NativeImage& nativeImage,
    const hle::HleRegistry& registry,
    const HleThunkTable& thunks) {
    NativeHleExecutionResult result;

    if (thunks.Size() == 0) {
        executor_.Run(
            context,
            nativeImage);
        return result;
    }

    NativeGuestMemoryAccess nativeMemory(
        nativeImage);

    bool resume = false;

    while (true) {
        std::uint64_t capturedRip = 0;

        {
            auto scope =
                trapBridge_.Arm(
                    thunks.BaseAddress(),
                    thunks.SlotSize(),
                    thunks.Size(),
                    executor_.EscapeAddress());

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
        }

        if (capturedRip == 0) {
            return result;
        }

        context.rip = capturedRip;

        const auto trapResult =
            HleTrapHandler::HandleInt3(
                context,
                nativeMemory,
                registry,
                thunks);

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
