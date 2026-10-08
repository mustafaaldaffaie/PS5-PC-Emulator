#include <ps5emu/elf/DynamicMetadata.hpp>
#include <ps5emu/elf/DynamicSymbol.hpp>
#include <ps5emu/elf/Elf64.hpp>
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

std::vector<std::byte> MakeSceElf() {
    std::vector<std::byte> bytes(0x600);

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
    Write<std::uint16_t>(bytes, 56, 3);

    WriteProgramHeader(
        bytes,
        64,
        1,
        5,
        0x100,
        0x400000,
        0x200,
        0x200,
        0x1000);

    WriteProgramHeader(
        bytes,
        64 + 56,
        2,
        4,
        0x180,
        0x400080,
        0xa0,
        0xa0,
        8);

    WriteProgramHeader(
        bytes,
        64 + 112,
        0x61000000,
        4,
        0x400,
        0,
        0x100,
        0x100,
        0x10);

    std::size_t dynamic = 0x180;
    WriteDynamicEntry(bytes, dynamic, 0x61000035, 0x00);
    dynamic += 16;
    WriteDynamicEntry(bytes, dynamic, 0x61000037, 21);
    dynamic += 16;
    WriteDynamicEntry(bytes, dynamic, 0x61000039, 0x40);
    dynamic += 16;
    WriteDynamicEntry(bytes, dynamic, 0x6100003b, 24);
    dynamic += 16;
    WriteDynamicEntry(bytes, dynamic, 0x6100003f, 48);
    dynamic += 16;
    WriteDynamicEntry(bytes, dynamic, 0x6100002f, 0x80);
    dynamic += 16;
    WriteDynamicEntry(bytes, dynamic, 0x61000031, 24);
    dynamic += 16;
    WriteDynamicEntry(bytes, dynamic, 0x61000033, 24);
    dynamic += 16;
    WriteDynamicEntry(bytes, dynamic, 1, 1);
    dynamic += 16;
    WriteDynamicEntry(bytes, dynamic, 0, 0);

    const char strings[] = "\0libkernel.prx\0NID_A\0";
    static_assert(sizeof(strings) - 1 == 21);
    std::memcpy(bytes.data() + 0x400, strings, sizeof(strings) - 1);

    const std::size_t symbol = 0x400 + 0x40 + 24;
    Write<std::uint32_t>(bytes, symbol + 0, 15);
    bytes[symbol + 4] = std::byte{0x12};
    bytes[symbol + 5] = std::byte{0};
    Write<std::uint16_t>(bytes, symbol + 6, 0);
    Write<std::uint64_t>(bytes, symbol + 8, 0);
    Write<std::uint64_t>(bytes, symbol + 16, 0);

    const std::size_t relocation = 0x400 + 0x80;
    Write<std::uint64_t>(bytes, relocation + 0, 0x400100);
    Write<std::uint64_t>(
        bytes,
        relocation + 8,
        (static_cast<std::uint64_t>(1) << 32) | 7);
    Write<std::int64_t>(bytes, relocation + 16, 0);

    return bytes;
}

} // namespace

int main() {
    {
        const auto bytes = MakeSceElf();
        const auto image = ps5emu::elf::Elf64::Parse(bytes);

        assert(image.sceDynamicDataSegment.has_value());

        const auto metadata =
            ps5emu::elf::DynamicMetadataParser::Parse(bytes, image);

        assert(metadata.stringTable.has_value());
        assert(metadata.stringTable->value == 0);
        assert(
            metadata.stringTable->kind ==
            ps5emu::elf::DynamicReferenceKind::SceDynamicDataOffset);
        assert(metadata.stringTableSize == 21);

        assert(metadata.symbolTable.has_value());
        assert(metadata.symbolTable->value == 0x40);
        assert(metadata.symbolTableSize == 48);
        assert(metadata.symbolEntrySize == 24);

        assert(metadata.neededLibraries.size() == 1);
        assert(metadata.neededLibraries[0] == "libkernel.prx");

        const auto symbol =
            ps5emu::elf::DynamicSymbolTable::Read(
                bytes,
                image,
                metadata,
                1);

        assert(symbol.name == "NID_A");
        assert(symbol.IsUndefined());

        const auto relocations =
            ps5emu::elf::RelocationTable::Parse(
                bytes,
                image,
                metadata);

        assert(relocations.size() == 1);
        assert(relocations[0].offset == 0x400100);
        assert(relocations[0].symbolIndex == 1);
        assert(relocations[0].type == 7);
    }

    {
        auto bytes = MakeSceElf();
        Write<std::uint32_t>(bytes, 64 + 112, 0);

        const auto image = ps5emu::elf::Elf64::Parse(bytes);
        assert(!image.sceDynamicDataSegment.has_value());

        assert(ThrowsRuntimeError([&] {
            static_cast<void>(
                ps5emu::elf::DynamicMetadataParser::Parse(bytes, image));
        }));
    }

    return 0;
}
