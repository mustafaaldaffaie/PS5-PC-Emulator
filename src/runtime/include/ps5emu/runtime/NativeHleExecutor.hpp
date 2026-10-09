#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <ps5emu/hle/HleRegistry.hpp>
#include <ps5emu/runtime/GuestCallDispatcher.hpp>
#include <ps5emu/runtime/HleThunkTable.hpp>
#include <ps5emu/runtime/NativeHleTrapBridge.hpp>
#include <ps5emu/runtime/NativeImageMaterializer.hpp>
#include <ps5emu/runtime/NativeLeafExecutor.hpp>
#include <ps5emu/runtime/NativeSyscallDispatcher.hpp>
#include <ps5emu/runtime/NativeSyscallInterceptor.hpp>

namespace ps5emu::runtime {

struct NativeHleExecutionResult {
    std::size_t handledTrapCount = 0;
    std::int64_t lastErrorCode = 0;
    std::size_t handledSyscallCount = 0;
    bool interceptedSyscall = false;
    std::uint64_t syscallNumber = 0;
    std::uint64_t syscallAddress = 0;
};

class NativeHleExecutor final {
public:
    NativeHleExecutor() = default;

    NativeHleExecutor(const NativeHleExecutor&) = delete;
    NativeHleExecutor& operator=(const NativeHleExecutor&) = delete;
    NativeHleExecutor(NativeHleExecutor&&) = delete;
    NativeHleExecutor& operator=(NativeHleExecutor&&) = delete;

    [[nodiscard]] NativeHleExecutionResult
    Run(SysvGuestContext& context,
        NativeImage& nativeImage,
        const hle::HleRegistry& registry,
        const HleThunkTable& thunks,
        std::span<const NativeSyscallTrap> syscallTraps = {},
        hle::GuestThreadAccess* threads = nullptr,
        hle::GuestFileSystemAccess* files = nullptr);

private:
    NativeLeafExecutor executor_;
    NativeHleTrapBridge trapBridge_;
    NativeSyscallDispatcher syscallDispatcher_;
};

} // namespace ps5emu::runtime
