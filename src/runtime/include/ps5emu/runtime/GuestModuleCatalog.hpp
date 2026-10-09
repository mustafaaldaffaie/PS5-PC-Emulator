#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include <ps5emu/elf/DynamicMetadata.hpp>
#include <ps5emu/elf/DynamicSymbol.hpp>
#include <ps5emu/elf/Elf64.hpp>
#include <ps5emu/elf/SceModuleMetadata.hpp>
#include <ps5emu/runtime/ModuleExportRegistry.hpp>

namespace ps5emu::runtime {

class GuestModuleCatalog final {
public:
    void Register(
        std::span<const std::byte> bytes,
        const elf::Image& image,
        const elf::DynamicMetadata& dynamic,
        const elf::SceModuleMetadata& metadata,
        std::uint64_t loadBias);

    [[nodiscard]] std::optional<std::uint64_t>
    Resolve(const elf::DynamicSymbol& symbol,
            const elf::SceModuleMetadata& importer) const;

    [[nodiscard]] const ModuleExportRegistry&
    Exports() const noexcept;

    [[nodiscard]] std::size_t ModuleCount() const noexcept;

private:
    ModuleExportRegistry exports_;
};

} // namespace ps5emu::runtime
