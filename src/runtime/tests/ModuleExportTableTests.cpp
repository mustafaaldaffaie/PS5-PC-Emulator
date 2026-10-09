#include <ps5emu/runtime/ModuleExportTable.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

template <typename T>
void Write(
    std::vector<std::byte>& bytes,
    std::size_t offset,
    T value) {
    assert(offset + sizeof(T) <= bytes.size());
    std::memcpy(
        bytes.data() + offset,
        &value,
        sizeof(T));
}

void WriteSymbol(
    std::vector<std::byte>& bytes,
    std::size_t tableOffset,
    std::uint32_t index,
    std::uint32_t nameOffset,
    std::uint8_t info,
    std::uint16_t sectionIndex,
    std::uint64_t value,
    std::uint64_t size) {
    const auto offset =
        tableOffset +
        static_cast<std::size_t>(index) * 24;

    Write(bytes, offset + 0, nameOffset);
    bytes[offset + 4] =
        static_cast<std::byte>(info);
    Write(bytes, offset + 6, sectionIndex);
    Write(bytes, offset + 8, value);
    Write(bytes, offset + 16, size);
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

} // namespace

int main() {
    constexpr std::size_t kStringOffset = 0x100;
    constexpr std::size_t kSymbolOffset = 0x200;

    std::vector<std::byte> bytes(0x400);

    const char strings[] =
        "\0foo\0bar\0weak\0tls\0";
    std::memcpy(
        bytes.data() + kStringOffset,
        strings,
        sizeof(strings));

    WriteSymbol(
        bytes,
        kSymbolOffset,
        1,
        1,
        0x12,
        1,
        0x400300,
        16);

    WriteSymbol(
        bytes,
        kSymbolOffset,
        2,
        5,
        0x11,
        0xfff1,
        0x42,
        8);

    WriteSymbol(
        bytes,
        kSymbolOffset,
        3,
        9,
        0x22,
        1,
        0x400320,
        4);

    WriteSymbol(
        bytes,
        kSymbolOffset,
        4,
        14,
        0x16,
        1,
        8,
        8);

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
            .value =
                0x400000 + kStringOffset,
            .kind =
                ps5emu::elf::DynamicReferenceKind::VirtualAddress,
        };
    metadata.stringTableSize =
        sizeof(strings);
    metadata.symbolTable =
        ps5emu::elf::DynamicTableReference{
            .value =
                0x400000 + kSymbolOffset,
            .kind =
                ps5emu::elf::DynamicReferenceKind::VirtualAddress,
        };
    metadata.symbolEntrySize = 24;
    metadata.symbolTableSize = 5 * 24;

    const auto table =
        ps5emu::runtime::ModuleExportTable::Build(
            bytes,
            image,
            metadata,
            0x100000);

    assert(table.Size() == 3);
    assert(table.Exports().size() == 3);

    const auto* foo =
        table.Find("foo");
    assert(foo != nullptr);
    assert(foo->symbolIndex == 1);
    assert(foo->address == 0x500300);
    assert(foo->size == 16);
    assert(foo->binding == 1);
    assert(foo->type == 2);

    const auto* bar =
        table.Find("bar");
    assert(bar != nullptr);
    assert(bar->address == 0x42);

    const auto* weak =
        table.Find("weak");
    assert(weak != nullptr);
    assert(weak->binding == 2);

    assert(table.Find("tls") == nullptr);
    assert(table.Find("missing") == nullptr);

    {
        auto malformed = metadata;
        malformed.symbolTableSize = 25;

        assert(ThrowsRuntimeError([&] {
            static_cast<void>(
                ps5emu::runtime::ModuleExportTable::Build(
                    bytes,
                    image,
                    malformed,
                    0));
        }));
    }

    {
        auto duplicate = bytes;
        WriteSymbol(
            duplicate,
            kSymbolOffset,
            3,
            1,
            0x12,
            1,
            0x400330,
            4);

        assert(ThrowsRuntimeError([&] {
            static_cast<void>(
                ps5emu::runtime::ModuleExportTable::Build(
                    duplicate,
                    image,
                    metadata,
                    0));
        }));
    }

    return 0;
}
