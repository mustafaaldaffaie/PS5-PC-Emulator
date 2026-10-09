#include <ps5emu/graphics/Pm4CommandStream.hpp>

#include <array>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

std::uint32_t Header(
    std::uint8_t opcode,
    std::size_t wordCount,
    std::uint8_t flags = 0) {
    assert(wordCount >= 2);
    assert(wordCount <= 0x4001);

    return 0xc0000000u |
        (static_cast<std::uint32_t>(
             wordCount - 2) << 16u) |
        (static_cast<std::uint32_t>(
             opcode) << 8u) |
        flags;
}

template <typename Function>
bool ThrowsRuntimeError(Function&& function) {
    try {
        function();
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

} // namespace

int main() {
    using ps5emu::graphics::Pm4CommandStream;

    {
        const std::array<std::uint32_t, 2> words{
            Header(0x10, 2, 0x05),
            0x12345678u,
        };

        const auto packets =
            Pm4CommandStream::Decode(words);

        assert(packets.size() == 1);
        assert(packets[0].opcode == 0x10);
        assert(packets[0].flags == 0x05);
        assert(packets[0].wordOffset == 0);
        assert(packets[0].words.size() == 2);
        assert(
            packets[0].words[1] ==
            0x12345678u);
    }

    {
        const std::vector<std::uint32_t> words{
            Header(0x22, 3),
            1u,
            2u,
            Header(0x33, 4, 0x80),
            3u,
            4u,
            5u,
        };

        const auto packets =
            Pm4CommandStream::Decode(words);

        assert(packets.size() == 2);
        assert(packets[0].wordOffset == 0);
        assert(packets[0].words.size() == 3);
        assert(packets[1].wordOffset == 3);
        assert(packets[1].words.size() == 4);
        assert(packets[1].opcode == 0x33);
        assert(packets[1].flags == 0x80);
    }

    {
        const std::array<std::uint32_t, 0>
            empty{};

        assert(
            Pm4CommandStream::Decode(
                empty).empty());
    }

    {
        const std::array<std::uint32_t, 2> words{
            0x00000000u,
            0u,
        };

        assert(ThrowsRuntimeError([&] {
            static_cast<void>(
                Pm4CommandStream::Decode(
                    words));
        }));
    }

    {
        const std::array<std::uint32_t, 2> words{
            Header(0x44, 4),
            0u,
        };

        assert(ThrowsRuntimeError([&] {
            static_cast<void>(
                Pm4CommandStream::Decode(
                    words));
        }));
    }

    {
        const std::vector<std::uint32_t> words{
            Header(0x55, 2),
            0u,
            Header(0x66, 3),
            0u,
        };

        assert(ThrowsRuntimeError([&] {
            static_cast<void>(
                Pm4CommandStream::Decode(
                    words));
        }));
    }

    return 0;
}
