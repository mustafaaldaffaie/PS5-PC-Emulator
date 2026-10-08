#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ps5emu::elf {

struct SceQualifiedImport {
    std::string nid;
    std::uint16_t libraryId = 0;
    std::uint16_t moduleId = 0;
};

class SceQualifiedSymbol final {
public:
    [[nodiscard]] static std::optional<SceQualifiedImport>
    Parse(std::string_view symbol);
};

} // namespace ps5emu::elf
