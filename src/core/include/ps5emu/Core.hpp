#pragma once

#include <string_view>

namespace ps5emu {

class Core final {
public:
    [[nodiscard]] static std::string_view Version() noexcept;
};

} // namespace ps5emu
