#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <ps5emu/hle/HleRegistry.hpp>
#include <ps5emu/memory/GuestMemory.hpp>
#include <ps5emu/runtime/GuestCallDispatcher.hpp>
#include <ps5emu/runtime/GuestThreadMemory.hpp>
#include <ps5emu/runtime/HleThunkTable.hpp>
#include <ps5emu/runtime/SceExecutablePreparer.hpp>

namespace ps5emu::runtime {

struct GuestExecutionOptions {
    std::uint64_t loadBias = 0;

    GuestThreadMemoryOptions threadMemory{
        .stackAddress = 0x00007ffe00000000ull,
        .stackSize = 8u * 1024u * 1024u,
        .tlsAddress = 0x00007ffd00000000ull,
        .stackGuard = 0,
    };

    HleThunkTableOptions thunks{};
};

struct PreparedGuestExecution {
    hle::HleRegistry registry;
    memory::GuestMemory memory;
    HleThunkTable thunks;
    PreparedSceImage image;
    GuestThreadMemoryLayout threadMemory;
    SysvGuestContext context;
};

class GuestExecutionBuilder final {
public:
    [[nodiscard]] static PreparedGuestExecution
    Prepare(std::span<const std::byte> executableBytes,
            const GuestExecutionOptions& options = {});
};

} // namespace ps5emu::runtime
