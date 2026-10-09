#include <ps5emu/runtime/ModuleExportRegistry.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string_view>
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
    std::uint64_t value) {
    const auto offset =
        tableOffset +
        static_cast<std::size_t>(index) * 24;

    Write(bytes, offset + 0, nameOffset);
    bytes[offset + 4] =
        static_cast<std::byte>(info);
    Write(bytes, offset + 6, sectionIndex);
    Write(bytes, offset + 8, value);
    Write<std::uint64_t>(bytes, offset + 16, 0);
}

ps5emu::runtime::ModuleExportTable
MakeExports(
    std::uint64_t loadBias,
    std::string_view qualifiedName) {
    constexpr std::size_t kStringOffset = 0x100;
    constexpr std::size_t kSymbolOffset = 0x200;

    std::vector<std::byte> bytes(0x400);

    std::vector<char> strings;
    strings.push_back('\0');
    strings.insert(
        strings.end(),
        qualifiedName.begin(),
        qualifiedName.end());
    strings.push_back('\0');

    const std::uint32_t plainOffset =
        static_cast<std::uint32_t>(strings.size());

    constexpr char kPlain[] = "plain";
    strings.insert(
        strings.end(),
        std::begin(kPlain),
        std::end(kPlain));

    std::memcpy(
        bytes.data() + kStringOffset,
        strings.data(),
        strings.size());

    WriteSymbol(
        bytes,
        kSymbolOffset,
        1,
        1,
        0x12,
        1,
        0x400300);

    WriteSymbol(
        bytes,
        kSymbolOffset,
        2,
        plainOffset,
        0x12,
        1,
        0x400320);

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
        strings.size();
    metadata.symbolTable =
        ps5emu::elf::DynamicTableReference{
            .value =
                0x400000 + kSymbolOffset,
            .kind =
                ps5emu::elf::DynamicReferenceKind::VirtualAddress,
        };
    metadata.symbolEntrySize = 24;
    metadata.symbolTableSize = 3 * 24;

    return ps5emu::runtime::ModuleExportTable::Build(
        bytes,
        image,
        metadata,
        loadBias);
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
    ps5emu::runtime::ModuleExportRegistry registry;

    registry.Register(
        "libAlpha",
        MakeExports(
            0x100000,
            "ABCDEFGHIJK#B#C"));

    registry.Register(
        "libBeta",
        MakeExports(
            0x200000,
            "LMNOPQRSTUV#D#E"));

    assert(registry.ModuleCount() == 2);
    assert(registry.ExportCount() == 4);

    const auto* raw =
        registry.FindByName(
            "libAlpha",
            "ABCDEFGHIJK#B#C");

    assert(raw != nullptr);
    assert(raw->address == 0x500300);

    const auto* nid =
        registry.FindByNid(
            "libAlpha",
            "ABCDEFGHIJK");

    assert(nid != nullptr);
    assert(nid->address == 0x500300);

    const auto resolved =
        registry.ResolveByNid(
            "libBeta",
            "LMNOPQRSTUV");

    assert(resolved.has_value());
    assert(*resolved == 0x600300);

    assert(
        registry.FindByName(
            "libAlpha",
            "plain") != nullptr);

    assert(
        !registry.ResolveByNid(
            "libAlpha",
            "LMNOPQRSTUV").has_value());

    assert(Throws([&] {
        registry.Register(
            "libAlpha",
            MakeExports(
                0,
                "ZZZZZZZZZZZ#B#C"));
    }));

    assert(Throws([&] {
        registry.Register(
            "",
            MakeExports(
                0,
                "ZZZZZZZZZZZ#B#C"));
    }));

    return 0;
}
