#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <ps5emu/elf/Elf64.hpp>

namespace ps5emu::elf {

struct DynamicMetadata {
    std::vector<std::string> neededLibraries;
    std::optional<std::uint64_t> stringTableAddress;
    std::uint64_t stringTableSize = 0;
    std::optional<std::uint64_t> symbolTableAddress;
    std::uint64_t symbolEntrySize = 0;
    std::optional<std::uint64_t> relaAddress;
    std::uint64_t relaSize = 0;
    std::uint64_t relaEntrySize = 0;
    std::optional<std::uint64_t> jumpRelocationAddress;
    std::uint64_t jumpRelocationSize = 0;
};

class DynamicMetadataParser final {
public:
    [[nodiscard]] static DynamicMetadata
    Parse(std::span<const std::byte> bytes, const Image& image);
};

} // namespace ps5emu::elf
