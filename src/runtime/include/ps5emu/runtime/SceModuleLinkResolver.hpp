#pragma once

#include <cstdint>
#include <optional>

#include <ps5emu/elf/DynamicSymbol.hpp>
#include <ps5emu/elf/SceModuleMetadata.hpp>
#include <ps5emu/runtime/ModuleExportRegistry.hpp>

namespace ps5emu::runtime {

class SceModuleLinkResolver final {
public:
    [[nodiscard]] static std::optional<std::uint64_t>
    Resolve(const elf::DynamicSymbol& symbol,
            const elf::SceModuleMetadata& metadata,
            const ModuleExportRegistry& modules);
};

} // namespace ps5emu::runtime
