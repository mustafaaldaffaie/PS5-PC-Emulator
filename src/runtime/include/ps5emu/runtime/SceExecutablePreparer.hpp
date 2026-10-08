#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <ps5emu/elf/SceModuleMetadata.hpp>
#include <ps5emu/hle/HleRegistry.hpp>
#include <ps5emu/memory/GuestMemory.hpp>
#include <ps5emu/runtime/ExecutableLinker.hpp>
#include <ps5emu/runtime/HleThunkTable.hpp>

namespace ps5emu::runtime {

struct ScePrepareOptions {
    std::uint64_t loadBias = 0;
};

struct PreparedSceImage {
    LinkedImage linked;
    elf::SceModuleMetadata moduleMetadata;
    std::size_t resolvedHleImportCount = 0;
    std::size_t unresolvedImportCount = 0;
    std::size_t newThunkCount = 0;
};

class SceExecutablePreparer final {
public:
    // Memory and thunk state are committed together. If any stage fails,
    // including thunk-arena installation, both caller-owned objects remain
    // unchanged.
    [[nodiscard]] static PreparedSceImage
    Prepare(std::span<const std::byte> bytes,
            memory::GuestMemory& memory,
            const hle::HleRegistry& registry,
            HleThunkTable& thunks,
            const ScePrepareOptions& options = {});
};

} // namespace ps5emu::runtime
