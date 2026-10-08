#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

#include <ps5emu/memory/GuestMemory.hpp>

namespace ps5emu::loader {

struct LoadedImage {
    std::uint64_t entryPoint = 0;
    std::size_t mappedSegmentCount = 0;
};

class ExecutableImageLoader final {
public:
    [[nodiscard]] static LoadedImage
    LoadElf(std::span<const std::byte> bytes,
            memory::GuestMemory& memory,
            std::uint64_t loadBias = 0);

    [[nodiscard]] static LoadedImage
    LoadElfFile(const std::filesystem::path& path,
                memory::GuestMemory& memory,
                std::uint64_t loadBias = 0);
};

} // namespace ps5emu::loader
