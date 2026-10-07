#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace ps5emu::elf {

struct Segment {
    std::uint64_t virtualAddress = 0;
    std::uint64_t memorySize = 0;
    std::uint64_t fileSize = 0;
    std::uint64_t fileOffset = 0;
    std::uint32_t flags = 0;
    std::uint64_t alignment = 0;
};

struct Image {
    std::uint64_t entryPoint = 0;
    std::vector<Segment> loadSegments;
};

class Elf64 final {
public:
    [[nodiscard]] static Image Parse(std::span<const std::byte> bytes);
    [[nodiscard]] static Image ParseFile(const std::filesystem::path& path);
};

} // namespace ps5emu::elf
