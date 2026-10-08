#include <ps5emu/runtime/SceExecutablePreparer.hpp>

#include <ps5emu/elf/DynamicMetadata.hpp>
#include <ps5emu/elf/Elf64.hpp>
#include <ps5emu/elf/ImportTable.hpp>
#include <ps5emu/runtime/SceImportResolver.hpp>

#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace ps5emu::runtime {

PreparedSceImage SceExecutablePreparer::Prepare(
    std::span<const std::byte> bytes,
    memory::GuestMemory& memory,
    const hle::HleRegistry& registry,
    HleThunkTable& thunks,
    const ScePrepareOptions& options) {
    const auto image = elf::Elf64::Parse(bytes);
    const auto dynamic =
        elf::DynamicMetadataParser::Parse(bytes, image);
    const auto moduleMetadata =
        elf::SceModuleMetadataParser::Parse(
            bytes,
            image,
            dynamic);
    const auto imports =
        elf::ImportTable::Parse(bytes, image, dynamic);

    const auto resolution =
        SceImportResolver::Resolve(
            imports,
            moduleMetadata,
            registry);

    auto stagedMemory = memory;
    auto stagedThunks = thunks;
    const auto initialThunkCount = stagedThunks.Size();

    std::unordered_map<std::uint32_t, std::uint64_t>
        externalAddresses;
    externalAddresses.reserve(resolution.bindings.size());

    for (const auto& binding : resolution.bindings) {
        if (binding.service == nullptr) {
            throw std::runtime_error(
                "Resolved HLE import has no service");
        }

        const auto address =
            stagedThunks.Bind(*binding.service);

        const auto [iterator, inserted] =
            externalAddresses.emplace(
                binding.symbolIndex,
                address);
        static_cast<void>(iterator);

        if (!inserted) {
            throw std::runtime_error(
                "Duplicate HLE binding for one symbol index");
        }
    }

    LinkOptions linkOptions;
    linkOptions.loadBias = options.loadBias;
    linkOptions.resolveExternal =
        [&externalAddresses](const elf::DynamicSymbol& symbol)
            -> std::optional<std::uint64_t> {
        const auto found =
            externalAddresses.find(symbol.index);
        if (found == externalAddresses.end()) {
            return std::nullopt;
        }

        return found->second;
    };

    auto linked =
        ExecutableLinker::Load(
            bytes,
            stagedMemory,
            linkOptions);

    stagedThunks.InstallOrUpdate(stagedMemory);

    PreparedSceImage result;
    result.linked = std::move(linked);
    result.moduleMetadata = moduleMetadata;
    result.resolvedHleImportCount =
        resolution.bindings.size();
    result.unresolvedImportCount =
        resolution.unresolved.size();
    result.newThunkCount =
        stagedThunks.Size() - initialThunkCount;

    memory = std::move(stagedMemory);
    thunks = std::move(stagedThunks);

    return result;
}

} // namespace ps5emu::runtime
