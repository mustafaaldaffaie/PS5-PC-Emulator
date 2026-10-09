#pragma once

#include <string>

#include <ps5emu/hle/HleRegistry.hpp>

namespace ps5emu::hle {

class KernelFile final {
public:
    static void Register(HleRegistry& registry,
                         std::string module);
};

} // namespace ps5emu::hle
