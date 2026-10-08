#pragma once

#include <cstddef>
#include <span>

#include <ps5emu/runtime/GuestExecutionBuilder.hpp>
#include <ps5emu/runtime/NativeImageMaterializer.hpp>

namespace ps5emu::runtime {

struct PreparedNativeExecution {
    PreparedGuestExecution guest;
    NativeImage nativeImage;
};

class NativeExecutionBuilder final {
public:
    [[nodiscard]] static PreparedNativeExecution
    Prepare(std::span<const std::byte> executableBytes,
            const GuestExecutionOptions& options = {});
};

} // namespace ps5emu::runtime
