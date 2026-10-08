#include <ps5emu/elf/DynamicSymbol.hpp>

#include <ps5emu/elf/ElfFileView.hpp>

#include <cstring>
#include <limits>
#include <stdexcept>

namespace ps5emu::elf {
namespace {

constexpr std::size_t kElf64SymbolSize = 24;

#pragma pack(push, 1)
struct Elf64Symbol {
    std::uint32_t name;
    std::uint8_t info;
    std::uint8_t other;
    std::uint16_t sectionIndex;
    std::uint64_t value;
    std::uint64_t size;
};
#pragma pack(pop)

static_assert(sizeof(Elf64Symbol) == kElf64SymbolSize);

DynamicTableReference AdvanceReference(
    DynamicTableReference reference,
    std::uint64_t offset) {
    if (reference.value >
        std::numeric_limits<std::uint64_t>::max() - offset) {
        throw std::runtime_error(
            "ELF dynamic table reference overflows");
    }

    reference.value += offset;
    return reference;
}

} // namespace

bool DynamicSymbol::IsUndefined() const noexcept {
    return sectionIndex == 0;
}

std::uint8_t DynamicSymbol::Binding() const noexcept {
    return static_cast<std::uint8_t>(info >> 4);
}

std::uint8_t DynamicSymbol::Type() const noexcept {
    return static_cast<std::uint8_t>(info & 0x0f);
}

DynamicSymbol DynamicSymbolTable::Read(
    std::span<const std::byte> bytes,
    const Image& image,
    const DynamicMetadata& metadata,
    std::uint32_t symbolIndex) {
    if (!metadata.symbolTable.has_value()) {
        throw std::runtime_error("ELF dynamic symbol table is missing");
    }

    if (!metadata.stringTable.has_value() ||
        metadata.stringTableSize == 0) {
        throw std::runtime_error(
            "ELF dynamic string table is required for symbols");
    }

    if (metadata.symbolEntrySize != kElf64SymbolSize) {
        throw std::runtime_error(
            "Unexpected ELF64 dynamic symbol entry size");
    }

    const auto index = static_cast<std::uint64_t>(symbolIndex);
    if (index >
        std::numeric_limits<std::uint64_t>::max() / kElf64SymbolSize) {
        throw std::runtime_error("ELF dynamic symbol index overflows");
    }

    const auto byteOffset = index * kElf64SymbolSize;

    if (metadata.symbolTableSize != 0 &&
        (byteOffset > metadata.symbolTableSize ||
         kElf64SymbolSize >
             metadata.symbolTableSize - byteOffset)) {
        throw std::runtime_error(
            "ELF dynamic symbol index exceeds the symbol table");
    }

    const auto symbolReference =
        AdvanceReference(*metadata.symbolTable, byteOffset);

    const ElfFileView view(bytes, image);
    const auto range =
        view.ResolveRange(symbolReference, kElf64SymbolSize);

    Elf64Symbol symbol{};
    std::memcpy(&symbol, range.data(), sizeof(symbol));

    return DynamicSymbol{
        .index = symbolIndex,
        .name = view.ReadString(
            *metadata.stringTable,
            metadata.stringTableSize,
            symbol.name),
        .info = symbol.info,
        .other = symbol.other,
        .sectionIndex = symbol.sectionIndex,
        .value = symbol.value,
        .size = symbol.size,
    };
}

} // namespace ps5emu::elf
