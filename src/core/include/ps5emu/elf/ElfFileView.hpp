#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include <ps5emu/elf/DynamicMetadata.hpp>
#include <ps5emu/elf/Elf64.hpp>

namespace ps5emu::elf {

class ElfFileView final {
public:
    ElfFileView(std::span<const std::byte> bytes, const Image& image);

    [[nodiscard]] std::size_t
    ResolveFileOffset(std::uint64_t virtualAddress,
                      std::size_t requiredSize) const;

    [[nodiscard]] std::size_t
    ResolveFileOffset(const DynamicTableReference& reference,
                      std::size_t requiredSize) const;

    [[nodiscard]] std::span<const std::byte>
    ResolveRange(std::uint64_t virtualAddress,
                 std::size_t size) const;

    [[nodiscard]] std::span<const std::byte>
    ResolveRange(const DynamicTableReference& reference,
                 std::size_t size) const;

    [[nodiscard]] std::string
    ReadString(const DynamicTableReference& table,
               std::uint64_t tableSize,
               std::uint64_t stringOffset) const;

private:
    std::span<const std::byte> bytes_;
    const Image& image_;
};

} // namespace ps5emu::elf
