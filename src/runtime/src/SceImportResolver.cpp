#include <ps5emu/runtime/SceImportResolver.hpp>

#include <ps5emu/elf/SceQualifiedSymbol.hpp>

namespace ps5emu::runtime {

std::optional<ImportIdentity> SceImportResolver::ResolveIdentity(
    const elf::ImportSymbol& import,
    const elf::SceModuleMetadata& metadata) {
    const auto qualified =
        elf::SceQualifiedSymbol::Parse(import.name);
    if (!qualified.has_value()) {
        return std::nullopt;
    }

    const auto* module =
        metadata.FindNeededModule(qualified->moduleId);
    if (module == nullptr) {
        return std::nullopt;
    }

    if (!metadata.importLibraries.empty() &&
        metadata.FindImportLibrary(qualified->libraryId) == nullptr) {
        return std::nullopt;
    }

    return ImportIdentity{
        .module = module->name,
        .nid = qualified->nid,
    };
}

ImportResolutionReport SceImportResolver::Resolve(
    std::span<const elf::ImportSymbol> imports,
    const elf::SceModuleMetadata& metadata,
    const hle::HleRegistry& registry) {
    return ImportResolver::Resolve(
        imports,
        registry,
        [&metadata](const elf::ImportSymbol& import) {
            return ResolveIdentity(import, metadata);
        });
}

} // namespace ps5emu::runtime
