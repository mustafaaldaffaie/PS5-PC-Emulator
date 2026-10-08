#pragma once

#include <string>
#include <string_view>

namespace ps5emu::hle {

class Nid final {
public:
    [[nodiscard]] static std::string Compute(std::string_view symbolName);

    [[nodiscard]] static bool
    IsValid(std::string_view nid) noexcept;
};

} // namespace ps5emu::hle
