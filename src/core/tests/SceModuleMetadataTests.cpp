#include <ps5emu/elf/DynamicMetadata.hpp>
#include <ps5emu/elf/Elf64.hpp>
#include <ps5emu/elf/SceModuleMetadata.hpp>
#include <ps5emu/elf/SceQualifiedSymbol.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

template <typename T>
void Write(std::vector<std::byte>& bytes, std::size_t offset, T value) {
    assert(offset + sizeof(T) <= bytes.size());
    std::memcpy(bytes.data() + offset, &value, sizeof(T));
}

template <typename Function>
bool ThrowsRuntimeError(Function&& function) {
    try {
        function();
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

void WriteProgramHeader(std::vector<std::byte>& bytes,
                        std::size_t offset,
                        std::uint32_t type,
                        std::uint32_t flags,
                        std::uint64_t fileOffset,
                        std::uint64_t virtualAddress,
                        std::uint64_t fileSize,
                        std::uint64_t memorySize,
                        std::uint64_t alignment) {
    Write<std::uint32_t>(bytes, offset + 0, type);
    Write<std::uint32_t>(bytes, offset + 4, flags);
    Write<std::uint64_t>(bytes, offset + 8, fileOffset);
    Write<std::uint64_t>(bytes, offset + 16, virtualAddress);
    Write<std::uint64_t>(bytes, offset + 24, 0);
    Write<std::uint64_t>(bytes, offset + 32, fileSize);
    Write<std::uint64_t>(bytes, offset + 40, memorySize);
    Write<std::uint64_t>(bytes, offset + 48, alignment);
}

void WriteDynamicEntry(std::vector<std::byte>& bytes,
                       std::size_t offset,
                       std::int64_t tag,
                       std::uint64_t value) {
    Write<std::int64_t>(bytes, offset, tag);
    Write<std::uint64_t>(bytes, offset + 8, value);
}

std::uint64_t PackModuleRecord(std::uint16_t id,
                               std::uint8_t major,
                               std::uint8_t minor,
                               std::uint32_t nameOffset) {
    return (static_cast<std::uint64_t>(id) << 48) |
           (static_cast<std::uint64_t>(major) << 40) |
           (static_cast<std::uint64_t>(minor) << 32) |
           nameOffset;
}

std::uint64_t PackLibraryRecord(std::uint16_t id,
                                std::uint16_t version,
                                std::uint32_t nameOffset) {
    return (static_cast<std::uint64_t>(id) << 48) |
           (static_cast<std::uint64_t>(version) << 32) |
           nameOffset;
}

std::vector<std::byte> MakeMetadataElf(bool nextGenerationTags) {
    std::vector<std::byte> bytes(0x800);

    bytes[0] = std::byte{0x7f};
    bytes[1] = std::byte{'E'};
    bytes[2] = std::byte{'L'};
    bytes[3] = std::byte{'F'};
    bytes[4] = std::byte{2};
    bytes[5] = std::byte{1};
    bytes[6] = std::byte{1};

    Write<std::uint16_t>(bytes, 16, 2);
    Write<std::uint16_t>(bytes, 18, 62);
    Write<std::uint32_t>(bytes, 20, 1);
    Write<std::uint64_t>(bytes, 24, 0x400000);
    Write<std::uint64_t>(bytes, 32, 64);
    Write<std::uint16_t>(bytes, 52, 64);
    Write<std::uint16_t>(bytes, 54, 56);
    Write<std::uint16_t>(bytes, 56, 2);

    WriteProgramHeader(
        bytes,
        64,
        1,
        5,
        0x100,
        0x400000,
        0x600,
        0x600,
        0x1000);

    WriteProgramHeader(
        bytes,
        64 + 56,
        2,
        4,
        0x180,
        0x400080,
        0x70,
        0x70,
        8);

    constexpr std::int64_t kModuleInfoLegacy = 0x6100000d;
    constexpr std::int64_t kNeededModuleLegacy = 0x6100000f;
    constexpr std::int64_t kExportLibraryLegacy = 0x61000013;
    constexpr std::int64_t kImportLibraryLegacy = 0x61000015;
    constexpr std::int64_t kModuleInfo = 0x61000043;
    constexpr std::int64_t kNeededModule = 0x61000045;
    constexpr std::int64_t kExportLibrary = 0x61000047;
    constexpr std::int64_t kImportLibrary = 0x61000049;

    const auto moduleInfoTag =
        nextGenerationTags ? kModuleInfo : kModuleInfoLegacy;
    const auto neededModuleTag =
        nextGenerationTags ? kNeededModule : kNeededModuleLegacy;
    const auto exportLibraryTag =
        nextGenerationTags ? kExportLibrary : kExportLibraryLegacy;
    const auto importLibraryTag =
        nextGenerationTags ? kImportLibrary : kImportLibraryLegacy;

    WriteDynamicEntry(bytes, 0x180, 5, 0x400300);
    WriteDynamicEntry(bytes, 0x190, 10, 35);
    WriteDynamicEntry(
        bytes,
        0x1a0,
        moduleInfoTag,
        PackModuleRecord(9, 1, 2, 1));
    WriteDynamicEntry(
        bytes,
        0x1b0,
        neededModuleTag,
        PackModuleRecord(2, 3, 4, 7));
    WriteDynamicEntry(
        bytes,
        0x1c0,
        importLibraryTag,
        PackLibraryRecord(1, 0x1234, 17));
    WriteDynamicEntry(
        bytes,
        0x1d0,
        exportLibraryTag,
        PackLibraryRecord(0, 0x0100, 30));
    WriteDynamicEntry(bytes, 0x1e0, 0, 0);

    const char strings[] =
        "\0eboot\0libkernel\0libSceKernel\0main\0";
    static_assert(sizeof(strings) - 1 == 35);
    std::memcpy(
        bytes.data() + 0x400,
        strings,
        sizeof(strings) - 1);

    return bytes;
}

void CheckMetadata(bool nextGenerationTags) {
    const auto bytes = MakeMetadataElf(nextGenerationTags);
    const auto image = ps5emu::elf::Elf64::Parse(bytes);
    const auto dynamic =
        ps5emu::elf::DynamicMetadataParser::Parse(bytes, image);
    const auto metadata =
        ps5emu::elf::SceModuleMetadataParser::Parse(
            bytes,
            image,
            dynamic);

    assert(metadata.module.has_value());
    assert(metadata.module->id == 9);
    assert(metadata.module->versionMajor == 1);
    assert(metadata.module->versionMinor == 2);
    assert(metadata.module->name == "eboot");

    assert(metadata.neededModules.size() == 1);
    assert(metadata.neededModules[0].id == 2);
    assert(metadata.neededModules[0].versionMajor == 3);
    assert(metadata.neededModules[0].versionMinor == 4);
    assert(metadata.neededModules[0].name == "libkernel");

    assert(metadata.importLibraries.size() == 1);
    assert(metadata.importLibraries[0].id == 1);
    assert(metadata.importLibraries[0].version == 0x1234);
    assert(metadata.importLibraries[0].name == "libSceKernel");

    assert(metadata.exportLibraries.size() == 1);
    assert(metadata.exportLibraries[0].id == 0);
    assert(metadata.exportLibraries[0].version == 0x0100);
    assert(metadata.exportLibraries[0].name == "main");

    assert(metadata.FindNeededModule(2) != nullptr);
    assert(metadata.FindNeededModule(99) == nullptr);
    assert(metadata.FindImportLibrary(1) != nullptr);
    assert(metadata.FindImportLibrary(99) == nullptr);
}

} // namespace

int main() {
    CheckMetadata(false);
    CheckMetadata(true);

    {
        const auto parsed =
            ps5emu::elf::SceQualifiedSymbol::Parse(
                "ABCDEFGHIJK#B#C");

        assert(parsed.has_value());
        assert(parsed->nid == "ABCDEFGHIJK");
        assert(parsed->libraryId == 1);
        assert(parsed->moduleId == 2);
    }

    {
        const auto parsed =
            ps5emu::elf::SceQualifiedSymbol::Parse("sceFoo");
        assert(!parsed.has_value());
    }

    assert(ThrowsRuntimeError([] {
        static_cast<void>(
            ps5emu::elf::SceQualifiedSymbol::Parse(
                "SHORT#B#C"));
    }));

    assert(ThrowsRuntimeError([] {
        static_cast<void>(
            ps5emu::elf::SceQualifiedSymbol::Parse(
                "ABCDEFGHIJK#_#C"));
    }));

    assert(ThrowsRuntimeError([] {
        static_cast<void>(
            ps5emu::elf::SceQualifiedSymbol::Parse(
                "ABCDEFGHIJK#BAAA#C"));
    }));

    {
        auto bytes = MakeMetadataElf(true);
        WriteDynamicEntry(
            bytes,
            0x1d0,
            0x61000045,
            PackModuleRecord(2, 5, 6, 7));

        const auto image = ps5emu::elf::Elf64::Parse(bytes);
        const auto dynamic =
            ps5emu::elf::DynamicMetadataParser::Parse(bytes, image);

        assert(ThrowsRuntimeError([&] {
            static_cast<void>(
                ps5emu::elf::SceModuleMetadataParser::Parse(
                    bytes,
                    image,
                    dynamic));
        }));
    }

    return 0;
}
