#include <ps5emu/runtime/GuestModuleCatalog.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

template <typename T>
void Write(std::vector<std::byte>& bytes,
           std::size_t offset,
           T value) {
    assert(offset + sizeof(T) <= bytes.size());
    std::memcpy(bytes.data() + offset, &value, sizeof(T));
}

struct Fixture {
    std::vector<std::byte> bytes;
    ps5emu::elf::Image image;
    ps5emu::elf::DynamicMetadata dynamic;
    ps5emu::elf::SceModuleMetadata metadata;
};

Fixture MakeModule() {
    constexpr std::size_t kStringOffset = 0x100;
    constexpr std::size_t kSymbolOffset = 0x200;

    Fixture fixture;
    fixture.bytes.resize(0x400);

    constexpr char kStrings[] =
        "\0ABCDEFGHIJK#D#E\0";

    std::memcpy(
        fixture.bytes.data() + kStringOffset,
        kStrings,
        sizeof(kStrings));

    Write<std::uint32_t>(
        fixture.bytes,
        kSymbolOffset + 24,
        1);
    fixture.bytes[kSymbolOffset + 28] =
        std::byte{0x12};
    Write<std::uint16_t>(
        fixture.bytes,
        kSymbolOffset + 30,
        1);
    Write<std::uint64_t>(
        fixture.bytes,
        kSymbolOffset + 32,
        0x400300);

    fixture.image.loadSegments.push_back(
        ps5emu::elf::Segment{
            .virtualAddress = 0x400000,
            .memorySize = fixture.bytes.size(),
            .fileSize = fixture.bytes.size(),
            .fileOffset = 0,
            .flags = 5,
            .alignment = 0x1000,
        });

    fixture.dynamic.stringTable =
        ps5emu::elf::DynamicTableReference{
            .value = 0x400000 + kStringOffset,
            .kind =
                ps5emu::elf::DynamicReferenceKind::VirtualAddress,
        };
    fixture.dynamic.stringTableSize =
        sizeof(kStrings);
    fixture.dynamic.symbolTable =
        ps5emu::elf::DynamicTableReference{
            .value = 0x400000 + kSymbolOffset,
            .kind =
                ps5emu::elf::DynamicReferenceKind::VirtualAddress,
        };
    fixture.dynamic.symbolEntrySize = 24;
    fixture.dynamic.symbolTableSize = 2 * 24;

    fixture.metadata.module =
        ps5emu::elf::SceModuleRecord{
            .id = 4,
            .versionMajor = 1,
            .versionMinor = 0,
            .name = "libAlpha",
        };

    return fixture;
}

template <typename Function>
bool Throws(Function&& function) {
    try {
        function();
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

} // namespace

int main() {
    auto module = MakeModule();

    ps5emu::runtime::GuestModuleCatalog catalog;
    catalog.Register(
        module.bytes,
        module.image,
        module.dynamic,
        module.metadata,
        0x100000);

    assert(catalog.ModuleCount() == 1);

    ps5emu::elf::SceModuleMetadata importer;
    importer.neededModules.push_back(
        ps5emu::elf::SceModuleRecord{
            .id = 2,
            .versionMajor = 1,
            .versionMinor = 0,
            .name = "libAlpha",
        });
    importer.importLibraries.push_back(
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
        catalog.Resolve(
            symbol,
            importer);

    assert(resolved.has_value());
    assert(*resolved == 0x500300);

    assert(Throws([&] {
        catalog.Register(
            module.bytes,
            module.image,
            module.dynamic,
            module.metadata,
            0);
    }));

    auto noMetadata = module.metadata;
    noMetadata.module.reset();

    assert(Throws([&] {
        ps5emu::runtime::GuestModuleCatalog other;
        other.Register(
            module.bytes,
            module.image,
            module.dynamic,
            noMetadata,
            0);
    }));

    auto missing = importer;
    missing.neededModules.clear();

    assert(
        !catalog.Resolve(
            symbol,
            missing).has_value());

    return 0;
}
