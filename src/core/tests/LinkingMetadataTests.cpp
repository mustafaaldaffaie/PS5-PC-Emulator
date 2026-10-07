#include <ps5emu/elf/DynamicMetadata.hpp>
#include <ps5emu/elf/DynamicSymbol.hpp>
#include <ps5emu/elf/Elf64.hpp>
#include <ps5emu/elf/ImportTable.hpp>
#include <ps5emu/elf/Relocation.hpp>

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

void WriteDynamicEntry(std::vector<std::byte>& bytes,
                       std::size_t offset,
                       std::int64_t tag,
                       std::uint64_t value) {
    Write<std::int64_t>(bytes, offset, tag);
    Write<std::uint64_t>(bytes, offset + 8, value);
}

std::vector<std::byte> MakeLinkingElf() {
    std::vector<std::byte> bytes(0x500);

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

    const std::size_t load = 64;
    Write<std::uint32_t>(bytes, load + 0, 1);
    Write<std::uint32_t>(bytes, load + 4, 5);
    Write<std::uint64_t>(bytes, load + 8, 0x100);
    Write<std::uint64_t>(bytes, load + 16, 0x400000);
    Write<std::uint64_t>(bytes, load + 32, 0x300);
    Write<std::uint64_t>(bytes, load + 40, 0x300);
    Write<std::uint64_t>(bytes, load + 48, 0x1000);

    const std::size_t dynamic = 64 + 56;
    Write<std::uint32_t>(bytes, dynamic + 0, 2);
    Write<std::uint32_t>(bytes, dynamic + 4, 4);
    Write<std::uint64_t>(bytes, dynamic + 8, 0x140);
    Write<std::uint64_t>(bytes, dynamic + 16, 0x400040);
    Write<std::uint64_t>(bytes, dynamic + 32, 0x90);
    Write<std::uint64_t>(bytes, dynamic + 40, 0x90);
    Write<std::uint64_t>(bytes, dynamic + 48, 8);

    WriteDynamicEntry(bytes, 0x140, 5, 0x4001a0);
    WriteDynamicEntry(bytes, 0x150, 10, 22);
    WriteDynamicEntry(bytes, 0x160, 6, 0x400120);
    WriteDynamicEntry(bytes, 0x170, 11, 24);
    WriteDynamicEntry(bytes, 0x180, 7, 0x400160);
    WriteDynamicEntry(bytes, 0x190, 8, 24);
    WriteDynamicEntry(bytes, 0x1a0, 9, 24);
    WriteDynamicEntry(bytes, 0x1b0, 1, 8);
    WriteDynamicEntry(bytes, 0x1c0, 0, 0);

    const std::size_t symbol = 0x220 + 24;
    Write<std::uint32_t>(bytes, symbol + 0, 1);
    bytes[symbol + 4] = std::byte{0x12};
    bytes[symbol + 5] = std::byte{0};
    Write<std::uint16_t>(bytes, symbol + 6, 0);
    Write<std::uint64_t>(bytes, symbol + 8, 0);
    Write<std::uint64_t>(bytes, symbol + 16, 0);

    const std::size_t relocation = 0x260;
    Write<std::uint64_t>(bytes, relocation + 0, 0x401000);
    Write<std::uint64_t>(
        bytes,
        relocation + 8,
        (static_cast<std::uint64_t>(1) << 32) | 7);
    Write<std::int64_t>(bytes, relocation + 16, 0);

    const char strings[] = "\0sceFoo\0libkernel.prx\0";
    static_assert(sizeof(strings) - 1 == 22);
    std::memcpy(bytes.data() + 0x2a0, strings, sizeof(strings) - 1);

    return bytes;
}

} // namespace

int main() {
    const auto bytes = MakeLinkingElf();
    const auto image = ps5emu::elf::Elf64::Parse(bytes);
    const auto metadata =
        ps5emu::elf::DynamicMetadataParser::Parse(bytes, image);

    assert(metadata.neededLibraries.size() == 1);
    assert(metadata.neededLibraries[0] == "libkernel.prx");

    const auto symbol =
        ps5emu::elf::DynamicSymbolTable::Read(
            bytes, image, metadata, 1);

    assert(symbol.index == 1);
    assert(symbol.name == "sceFoo");
    assert(symbol.IsUndefined());
    assert(symbol.Binding() == 1);
    assert(symbol.Type() == 2);

    const auto relocations =
        ps5emu::elf::RelocationTable::Parse(
            bytes, image, metadata);

    assert(relocations.size() == 1);
    assert(relocations[0].offset == 0x401000);
    assert(relocations[0].symbolIndex == 1);
    assert(relocations[0].type == 7);
    assert(relocations[0].addend == 0);
    assert(!relocations[0].procedureLinkage);

    const auto imports =
        ps5emu::elf::ImportTable::Parse(
            bytes, image, metadata);

    assert(imports.size() == 1);
    assert(imports[0].symbolIndex == 1);
    assert(imports[0].name == "sceFoo");
    assert(imports[0].binding == 1);
    assert(imports[0].type == 2);
    assert(imports[0].relocations.size() == 1);

    auto malformed = metadata;
    malformed.symbolEntrySize = 8;
    assert(ThrowsRuntimeError([&] {
        static_cast<void>(
            ps5emu::elf::DynamicSymbolTable::Read(
                bytes, image, malformed, 1));
    }));

    return 0;
}
