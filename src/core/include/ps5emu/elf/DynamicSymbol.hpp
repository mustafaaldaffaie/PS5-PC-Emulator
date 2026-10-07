#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include <ps5emu/elf/DynamicMetadata.hpp>
#include <ps5emu/elf/Elf64.hpp>

namespace ps5emu::elf {

struct DynamicSymbol {
    std::uint32_t index = 0;
    std::string name;
    std::uint8_t info = 0;
    std::uint8_t other = 0;
    std::uint16_t sectionIndex = 0;
    std::uint64_t value = 0;
    std::uint64_t size = 0;

    [[nodiscard]] bool IsUndefined() const noexcept;
    [[nodiscard]] std::uint8_t Binding() const noexcept;
    [[nodiscard]] std::uint8_t Type() const noexcept;
};

class DynamicSymbolTable final {
public:
    [[nodiscard]] static DynamicSymbol
    Read(std::span<const std::byte> bytes,
         const Image& image,
         const DynamicMetadata& metadata,
         std::uint32_t symbolIndex);
};

} // namespace ps5emu::elf
