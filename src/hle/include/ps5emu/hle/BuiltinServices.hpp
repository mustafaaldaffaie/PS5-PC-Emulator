#pragma once

#include <ps5emu/hle/HleRegistry.hpp>

namespace ps5emu::hle {

class BuiltinServices final {
public:
    static void Register(HleRegistry& registry);

    [[nodiscard]] static HleRegistry CreateRegistry();
};

} // namespace ps5emu::hle
