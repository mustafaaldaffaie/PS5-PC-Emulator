#pragma once

#include <cstdint>

#include <ps5emu/hle/GuestMemoryAccess.hpp>
#include <ps5emu/hle/HleRegistry.hpp>
#include <ps5emu/memory/GuestMemory.hpp>
#include <ps5emu/runtime/GuestCallDispatcher.hpp>
#include <ps5emu/runtime/HleThunkTable.hpp>

namespace ps5emu::runtime {

struct HleTrapResult {
    bool handled = false;
    std::int64_t errorCode = 0;
};

class HleTrapHandler final {
public:
    // Handles an x86 INT3 after the CPU has advanced RIP by one byte.
    // On a resolved HLE thunk, the guest call is dispatched and the
    // pushed return address is consumed as if the thunk had returned.
    [[nodiscard]] static HleTrapResult
    HandleInt3(SysvGuestContext& context,
               hle::GuestMemoryAccess& memory,
               const hle::HleRegistry& registry,
               const HleThunkTable& thunks,
               hle::GuestThreadAccess* threads = nullptr);

    [[nodiscard]] static HleTrapResult
    HandleInt3(SysvGuestContext& context,
               memory::GuestMemory& memory,
               const hle::HleRegistry& registry,
               const HleThunkTable& thunks,
               hle::GuestThreadAccess* threads = nullptr);
};

} // namespace ps5emu::runtime
