#include <ps5emu/runtime/GuestModuleLoader.hpp>

#include <stdexcept>
#include <utility>

#include <ps5emu/elf/DynamicMetadata.hpp>
#include <ps5emu/elf/Elf64.hpp>
#include <ps5emu/elf/SceModuleMetadata.hpp>

namespace ps5emu::runtime {

LoadedGuestModule GuestModuleLoader::Load(
    std::span<const std::byte> bytes,
    const GuestModuleLoadOptions& options) {
    const auto image =
        elf::Elf64::Parse(bytes);
    const auto dynamic =
        elf::DynamicMetadataParser::Parse(
            bytes,
            image);
    const auto metadata =
        elf::SceModuleMetadataParser::Parse(
            bytes,
            image,
            dynamic);

    if (!metadata.module.has_value()) {
        throw std::runtime_error(
            "Guest module loader requires SCE module metadata");
    }

    auto stagedMemory = memory_;
    auto stagedCatalog = catalog_;

    LinkOptions linkOptions;
    linkOptions.loadBias =
        options.loadBias;

    linkOptions.resolveExternal =
        [&](const elf::DynamicSymbol& symbol)
            -> std::optional<std::uint64_t> {
        const auto moduleAddress =
            stagedCatalog.Resolve(
                symbol,
                metadata);

        if (moduleAddress.has_value()) {
            return moduleAddress;
        }

        if (options.fallbackResolver) {
            return options.fallbackResolver(
                symbol);
        }

        return std::nullopt;
    };

    auto linked =
        ExecutableLinker::Load(
            bytes,
            stagedMemory,
            linkOptions);

    stagedCatalog.Register(
        bytes,
        image,
        dynamic,
        metadata,
        options.loadBias);

    memory_ =
        std::move(stagedMemory);
    catalog_ =
        std::move(stagedCatalog);

    return LoadedGuestModule{
        .name = metadata.module->name,
        .linked = std::move(linked),
        .metadata = metadata,
    };
}

const memory::GuestMemory&
GuestModuleLoader::Memory() const noexcept {
    return memory_;
}

const GuestModuleCatalog&
GuestModuleLoader::Catalog() const noexcept {
    return catalog_;
}

} // namespace ps5emu::runtime
