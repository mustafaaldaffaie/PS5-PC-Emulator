#pragma once

#include <cstddef>
#include <cstdint>

#include <ps5emu/hle/HleRegistry.hpp>
#include <ps5emu/runtime/GuestCallDispatcher.hpp>
#include <ps5emu/runtime/HleThunkTable.hpp>
#include <ps5emu/runtime/NativeHleTrapBridge.hpp>
#include <ps5emu/runtime/NativeImageMaterializer.hpp>
#include <ps5emu/runtime/NativeLeafExecutor.hpp>

namespace ps5emu::runtime {

struct NativeHleExecutionResult {
    std::size_t handledTrapCount = 0;
    std::int64_t lastErrorCode = 0;
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
        const HleThunkTable& thunks);

private:
    NativeLeafExecutor executor_;
    NativeHleTrapBridge trapBridge_;
};

} // namespace ps5emu::runtime
