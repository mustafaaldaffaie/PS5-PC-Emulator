#include <ps5emu/graphics/Pm4CommandStream.hpp>

#include <stdexcept>

namespace ps5emu::graphics {
namespace {

constexpr std::uint32_t kPacketTypeMask = 0xc0000000u;
constexpr std::uint32_t kPacketType3 = 0xc0000000u;
constexpr std::uint32_t kCountMask = 0x3fffu;
constexpr std::uint32_t kOpcodeMask = 0xffu;
constexpr std::uint32_t kFlagsMask = 0xffu;

std::size_t PacketWordCount(std::uint32_t header) {
    const auto encodedCount =
        (header >> 16u) & kCountMask;

    return static_cast<std::size_t>(
        encodedCount) + 2u;
}

} // namespace

std::vector<Pm4PacketView>
Pm4CommandStream::Decode(
    std::span<const std::uint32_t> words) {
    std::vector<Pm4PacketView> packets;

    std::size_t offset = 0;

    while (offset < words.size()) {
        const auto header = words[offset];

        if ((header & kPacketTypeMask) != kPacketType3) {
            throw std::runtime_error(
                "Unsupported PM4 packet type");
        }

        const auto packetWordCount =
            PacketWordCount(header);

        if (packetWordCount < 2) {
            throw std::runtime_error(
                "Invalid PM4 packet word count");
        }

        if (packetWordCount >
            words.size() - offset) {
            throw std::runtime_error(
                "PM4 packet extends past the command stream");
        }

        packets.push_back(Pm4PacketView{
            .opcode = static_cast<std::uint8_t>(
                (header >> 8u) & kOpcodeMask),
            .flags = static_cast<std::uint8_t>(
                header & kFlagsMask),
            .wordOffset = offset,
            .words = words.subspan(
                offset,
                packetWordCount),
        });

        offset += packetWordCount;
    }

    return packets;
}

} // namespace ps5emu::graphics
