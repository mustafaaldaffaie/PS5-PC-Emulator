#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <ps5emu/elf/DynamicMetadata.hpp>
#include <ps5emu/elf/Elf64.hpp>

namespace ps5emu::elf {

struct SceModuleRecord {
    std::uint16_t id = 0;
    std::uint8_t versionMajor = 0;
    std::uint8_t versionMinor = 0;
    std::string name;
};

struct SceLibraryRecord {
    std::uint16_t id = 0;
    std::uint16_t version = 0;
    std::string name;
};

struct SceModuleMetadata {
    std::optional<SceModuleRecord> module;
    std::vector<SceModuleRecord> neededModules;
    std::vector<SceLibraryRecord> exportLibraries;
    std::vector<SceLibraryRecord> importLibraries;

    [[nodiscard]] const SceModuleRecord*
    FindNeededModule(std::uint16_t id) const noexcept;

    [[nodiscard]] const SceLibraryRecord*
    FindImportLibrary(std::uint16_t id) const noexcept;
};

class SceModuleMetadataParser final {
public:
    [[nodiscard]] static SceModuleMetadata
    Parse(std::span<const std::byte> bytes,
          const Image& image,
          const DynamicMetadata& dynamic);
};

} // namespace ps5emu::elf
