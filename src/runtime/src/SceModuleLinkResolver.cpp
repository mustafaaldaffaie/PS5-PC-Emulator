#include <ps5emu/runtime/SceModuleLinkResolver.hpp>

#include <ps5emu/elf/SceQualifiedSymbol.hpp>

namespace ps5emu::runtime {

std::optional<std::uint64_t>
SceModuleLinkResolver::Resolve(
    const elf::DynamicSymbol& symbol,
    const elf::SceModuleMetadata& metadata,
    const ModuleExportRegistry& modules) {
    if (!symbol.IsUndefined()) {
        return std::nullopt;
    }

    const auto qualified =
        elf::SceQualifiedSymbol::Parse(symbol.name);

    if (!qualified.has_value()) {
        return std::nullopt;
    }

    const auto* module =
        metadata.FindNeededModule(qualified->moduleId);

    if (module == nullptr) {
        return std::nullopt;
    }

    if (!metadata.importLibraries.empty() &&
        metadata.FindImportLibrary(
            qualified->libraryId) == nullptr) {
        return std::nullopt;
    }

    return modules.ResolveByNid(
        module->name,
        qualified->nid);
}

} // namespace ps5emu::runtime
