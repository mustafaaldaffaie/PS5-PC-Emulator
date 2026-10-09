#include <ps5emu/runtime/GuestModuleCatalog.hpp>

#include <stdexcept>

#include <ps5emu/runtime/ModuleExportTable.hpp>
#include <ps5emu/runtime/SceModuleLinkResolver.hpp>

namespace ps5emu::runtime {

void GuestModuleCatalog::Register(
    std::span<const std::byte> bytes,
    const elf::Image& image,
    const elf::DynamicMetadata& dynamic,
    const elf::SceModuleMetadata& metadata,
    std::uint64_t loadBias) {
    if (!metadata.module.has_value()) {
        throw std::runtime_error(
            "Guest module catalog requires SCE module metadata");
    }

    auto table =
        ModuleExportTable::Build(
            bytes,
            image,
            dynamic,
            loadBias);

    exports_.Register(
        metadata.module->name,
        std::move(table));
}

std::optional<std::uint64_t>
GuestModuleCatalog::Resolve(
    const elf::DynamicSymbol& symbol,
    const elf::SceModuleMetadata& importer) const {
    return SceModuleLinkResolver::Resolve(
        symbol,
        importer,
        exports_);
}

const ModuleExportRegistry&
GuestModuleCatalog::Exports() const noexcept {
    return exports_;
}

std::size_t
GuestModuleCatalog::ModuleCount() const noexcept {
    return exports_.ModuleCount();
}

} // namespace ps5emu::runtime
