#pragma once

#include <cstdint>

#include <ps5emu/runtime/GuestCallDispatcher.hpp>
#include <ps5emu/runtime/NativeImageMaterializer.hpp>
#include <ps5emu/runtime/NativeMemoryRegion.hpp>

namespace ps5emu::runtime {

// Executes one integer/GPR-oriented x86-64 guest leaf routine on the guest
// stack. The routine must return with RET. When the host OS exposes user-mode
// FSGSBASE, a nonzero guest FS base is installed only while guest code runs and
// the host FS base is restored before returning to C++.
// Host floating-point/SIMD state is saved and restored, but guest SIMD state is
// not yet modeled in SysvGuestContext.
class NativeLeafExecutor final {
public:
    NativeLeafExecutor();

    NativeLeafExecutor(const NativeLeafExecutor&) = delete;
    NativeLeafExecutor& operator=(const NativeLeafExecutor&) = delete;
    NativeLeafExecutor(NativeLeafExecutor&&) = delete;
    NativeLeafExecutor& operator=(NativeLeafExecutor&&) = delete;

    [[nodiscard]] static bool
    SupportsGuestFsBase() noexcept;

    void Run(SysvGuestContext& context,
             const NativeImage& nativeImage);

private:
    struct State {
        SysvGuestContext input;
        SysvGuestContext output;
        std::uint64_t hostRsp = 0;
        std::uint64_t hostRflags = 0;
        std::uint64_t hostFsBase = 0;
        std::uint64_t savedGuestRax = 0;
    };

    void BuildTrampoline();

    State state_{};
    NativeMemoryRegion code_;
};

} // namespace ps5emu::runtime
