#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ps5emu::graphics {

struct Pm4PacketView {
    std::uint8_t opcode = 0;
    std::uint8_t flags = 0;
    std::size_t wordOffset = 0;
    std::span<const std::uint32_t> words;
};

class Pm4CommandStream final {
public:
    [[nodiscard]] static std::vector<Pm4PacketView>
    Decode(std::span<const std::uint32_t> words);
};

} // namespace ps5emu::graphics
