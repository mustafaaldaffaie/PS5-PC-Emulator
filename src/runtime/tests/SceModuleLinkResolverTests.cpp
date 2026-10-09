#include <ps5emu/runtime/SceModuleLinkResolver.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

template <typename T>
void Write(std::vector<std::byte>& bytes,
           std::size_t offset,
           T value) {
    assert(offset + sizeof(T) <= bytes.size());
    std::memcpy(bytes.data() + offset, &value, sizeof(T));
}

ps5emu::runtime::ModuleExportTable MakeExports() {
    constexpr std::size_t kStringOffset = 0x100;
    constexpr std::size_t kSymbolOffset = 0x200;

    std::vector<std::byte> bytes(0x400);
    constexpr char kName[] = "\0ABCDEFGHIJK#D#E\0";
    std::memcpy(bytes.data() + kStringOffset,
                kName,
                sizeof(kName));

    Write<std::uint32_t>(bytes, kSymbolOffset + 24, 1);
    bytes[kSymbolOffset + 28] = std::byte{0x12};
    Write<std::uint16_t>(bytes, kSymbolOffset + 30, 1);
    Write<std::uint64_t>(bytes, kSymbolOffset + 32, 0x400300);

    ps5emu::elf::Image image;
    image.loadSegments.push_back(
        ps5emu::elf::Segment{
            .virtualAddress = 0x400000,
            .memorySize = bytes.size(),
            .fileSize = bytes.size(),
            .fileOffset = 0,
            .flags = 5,
            .alignment = 0x1000,
        });

    ps5emu::elf::DynamicMetadata metadata;
    metadata.stringTable =
        ps5emu::elf::DynamicTableReference{
            .value = 0x400000 + kStringOffset,
            .kind =
                ps5emu::elf::DynamicReferenceKind::VirtualAddress,
        };
    metadata.stringTableSize = sizeof(kName);
    metadata.symbolTable =
        ps5emu::elf::DynamicTableReference{
            .value = 0x400000 + kSymbolOffset,
            .kind =
                ps5emu::elf::DynamicReferenceKind::VirtualAddress,
        };
    metadata.symbolEntrySize = 24;
    metadata.symbolTableSize = 2 * 24;

    return ps5emu::runtime::ModuleExportTable::Build(
        bytes,
        image,
        metadata,
        0x100000);
}

} // namespace

int main() {
    ps5emu::runtime::ModuleExportRegistry modules;
    modules.Register("libAlpha", MakeExports());

    ps5emu::elf::SceModuleMetadata metadata;
    metadata.neededModules.push_back(
        ps5emu::elf::SceModuleRecord{
            .id = 2,
            .versionMajor = 1,
            .versionMinor = 0,
            .name = "libAlpha",
        });

    metadata.importLibraries.push_back(
        ps5emu::elf::SceLibraryRecord{
            .id = 1,
            .version = 1,
            .name = "libImport",
        });

    ps5emu::elf::DynamicSymbol symbol{
        .index = 1,
        .name = "ABCDEFGHIJK#B#C",
        .info = 0x12,
        .sectionIndex = 0,
    };

    const auto resolved =
        ps5emu::runtime::SceModuleLinkResolver::Resolve(
            symbol,
            metadata,
            modules);

    assert(resolved.has_value());
    assert(*resolved == 0x500300);

    auto missingModule = metadata;
    missingModule.neededModules.clear();
    assert(!ps5emu::runtime::SceModuleLinkResolver::Resolve(
        symbol,
        missingModule,
        modules).has_value());

    auto missingLibrary = metadata;
    missingLibrary.importLibraries.clear();
    missingLibrary.importLibraries.push_back(
        ps5emu::elf::SceLibraryRecord{
            .id = 7,
            .version = 1,
            .name = "other",
        });
    assert(!ps5emu::runtime::SceModuleLinkResolver::Resolve(
        symbol,
        missingLibrary,
        modules).has_value());

    auto plain = symbol;
    plain.name = "plain";
    assert(!ps5emu::runtime::SceModuleLinkResolver::Resolve(
        plain,
        metadata,
        modules).has_value());

    auto defined = symbol;
    defined.sectionIndex = 1;
    assert(!ps5emu::runtime::SceModuleLinkResolver::Resolve(
        defined,
        metadata,
        modules).has_value());

    return 0;
}
