#include <ps5emu/runtime/NativeExecutionBuilder.hpp>

#include <stdexcept>
#include <utility>

#include <ps5emu/runtime/NativeInstructionGuard.hpp>
#include <ps5emu/runtime/NativeSyscallInterceptor.hpp>

namespace ps5emu::runtime {

PreparedNativeExecution NativeExecutionBuilder::Prepare(
    std::span<const std::byte> executableBytes,
    const GuestExecutionOptions& options) {
    auto guest =
        GuestExecutionBuilder::Prepare(
            executableBytes,
            options);

    auto syscallTraps =
        NativeSyscallInterceptor::Rewrite(
            guest.memory);

    NativeInstructionGuard::Validate(
        guest.memory);

    auto nativeImage =
        NativeImageMaterializer::Materialize(
            guest.memory);

    if (!nativeImage.Contains(
            guest.context.rip,
            1)) {
        throw std::runtime_error(
            "Guest entry point is not present in the native image");
    }

    if (guest.context.rsp == 0 ||
        !nativeImage.Contains(
            guest.context.rsp - 1,
            1)) {
        throw std::runtime_error(
            "Guest stack pointer is not backed by native memory");
    }

    if (guest.context.fsBase != 0 &&
        !nativeImage.Contains(
            guest.context.fsBase,
            sizeof(std::uint64_t))) {
        throw std::runtime_error(
            "Guest FS base is not backed by native TLS memory");
    }

    return PreparedNativeExecution{
        .guest = std::move(guest),
        .nativeImage = std::move(nativeImage),
        .syscallTraps = std::move(syscallTraps),
    };
}

} // namespace ps5emu::runtime
