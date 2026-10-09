#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <ps5emu/elf/DynamicMetadata.hpp>
#include <ps5emu/elf/Elf64.hpp>

namespace ps5emu::runtime {

struct ModuleExport {
    std::uint32_t symbolIndex = 0;
    std::string name;
    std::uint64_t address = 0;
    std::uint64_t size = 0;
    std::uint8_t binding = 0;
    std::uint8_t type = 0;
};

class ModuleExportTable final {
public:
    [[nodiscard]] static ModuleExportTable
    Build(std::span<const std::byte> bytes,
          const elf::Image& image,
          const elf::DynamicMetadata& metadata,
          std::uint64_t loadBias);

    [[nodiscard]] const ModuleExport*
    Find(std::string_view name) const noexcept;

    [[nodiscard]] const std::vector<ModuleExport>&
    Exports() const noexcept;

    [[nodiscard]] std::size_t Size() const noexcept;

private:
    std::vector<ModuleExport> exports_;
    std::unordered_map<std::string, std::size_t> byName_;
};

} // namespace ps5emu::runtime
