#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <ps5emu/elf/Elf64.hpp>

namespace ps5emu::elf {

enum class DynamicReferenceKind : std::uint8_t {
    VirtualAddress,
    SceDynamicDataOffset
};

struct DynamicTableReference {
    std::uint64_t value = 0;
    DynamicReferenceKind kind = DynamicReferenceKind::VirtualAddress;
};

struct DynamicMetadata {
    std::vector<std::string> neededLibraries;
    std::optional<DynamicTableReference> stringTable;
    std::uint64_t stringTableSize = 0;
    std::optional<DynamicTableReference> symbolTable;
    std::uint64_t symbolTableSize = 0;
    std::uint64_t symbolEntrySize = 0;
    std::optional<DynamicTableReference> relaTable;
    std::uint64_t relaSize = 0;
    std::uint64_t relaEntrySize = 0;
    std::optional<DynamicTableReference> jumpRelocationTable;
    std::uint64_t jumpRelocationSize = 0;
    std::optional<std::uint64_t> pltRelocationType;
};

class DynamicMetadataParser final {
public:
    [[nodiscard]] static DynamicMetadata
    Parse(std::span<const std::byte> bytes, const Image& image);
};

} // namespace ps5emu::elf
