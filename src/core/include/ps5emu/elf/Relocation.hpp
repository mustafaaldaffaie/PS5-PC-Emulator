#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <ps5emu/elf/DynamicMetadata.hpp>
#include <ps5emu/elf/Elf64.hpp>

namespace ps5emu::elf {

struct Relocation {
    std::uint64_t offset = 0;
    std::uint32_t symbolIndex = 0;
    std::uint32_t type = 0;
    std::int64_t addend = 0;
    bool procedureLinkage = false;
};

class RelocationTable final {
public:
    [[nodiscard]] static std::vector<Relocation>
    Parse(std::span<const std::byte> bytes,
          const Image& image,
          const DynamicMetadata& metadata);
};

} // namespace ps5emu::elf
