#pragma once

#include <optional>
#include <span>

#include <ps5emu/elf/ImportTable.hpp>
#include <ps5emu/elf/SceModuleMetadata.hpp>
#include <ps5emu/hle/HleRegistry.hpp>
#include <ps5emu/runtime/ImportResolver.hpp>

namespace ps5emu::runtime {

class SceImportResolver final {
public:
    [[nodiscard]] static std::optional<ImportIdentity>
    ResolveIdentity(
        const elf::ImportSymbol& import,
        const elf::SceModuleMetadata& metadata);

    [[nodiscard]] static ImportResolutionReport
    Resolve(std::span<const elf::ImportSymbol> imports,
            const elf::SceModuleMetadata& metadata,
            const hle::HleRegistry& registry);
};

} // namespace ps5emu::runtime
