#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <ps5emu/elf/DynamicMetadata.hpp>
#include <ps5emu/elf/Elf64.hpp>
#include <ps5emu/elf/Relocation.hpp>

namespace ps5emu::elf {

struct ImportSymbol {
    std::uint32_t symbolIndex = 0;
    std::string name;
    std::uint8_t binding = 0;
    std::uint8_t type = 0;
    std::vector<Relocation> relocations;
};

class ImportTable final {
public:
    [[nodiscard]] static std::vector<ImportSymbol>
    Parse(std::span<const std::byte> bytes,
          const Image& image,
          const DynamicMetadata& metadata);
};

} // namespace ps5emu::elf
