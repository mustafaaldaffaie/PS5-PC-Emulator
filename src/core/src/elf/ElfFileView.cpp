#include <ps5emu/elf/ElfFileView.hpp>

#include <limits>
#include <stdexcept>

namespace ps5emu::elf {
namespace {

std::size_t CheckedSize(std::uint64_t value, const char* message) {
    if (value > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error(message);
    }

    return static_cast<std::size_t>(value);
}

} // namespace

ElfFileView::ElfFileView(std::span<const std::byte> bytes,
                         const Image& image)
    : bytes_(bytes),
      image_(image) {
}

std::size_t ElfFileView::ResolveFileOffset(
    std::uint64_t virtualAddress,
    std::size_t requiredSize) const {
    for (const auto& segment : image_.loadSegments) {
        if (virtualAddress < segment.virtualAddress) {
            continue;
        }

        const auto delta = virtualAddress - segment.virtualAddress;
        if (delta > segment.fileSize) {
            continue;
        }

        const auto available =
            CheckedSize(segment.fileSize - delta,
                        "ELF file-backed segment range is too large");
        if (requiredSize > available) {
            continue;
        }

        if (segment.fileOffset >
            std::numeric_limits<std::uint64_t>::max() - delta) {
            throw std::runtime_error("ELF file offset overflows");
        }

        const auto fileOffset = segment.fileOffset + delta;
        const auto hostOffset =
            CheckedSize(fileOffset,
                        "ELF file offset is too large for this host");

        if (hostOffset > bytes_.size() ||
            requiredSize > bytes_.size() - hostOffset) {
            throw std::runtime_error(
                "ELF virtual address resolves outside the input file");
        }

        return hostOffset;
    }

    throw std::runtime_error(
        "ELF virtual address is not backed by a load segment");
}

std::span<const std::byte> ElfFileView::ResolveRange(
    std::uint64_t virtualAddress,
    std::size_t size) const {
    const auto offset = ResolveFileOffset(virtualAddress, size);
    return bytes_.subspan(offset, size);
}

std::string ElfFileView::ReadString(
    std::uint64_t tableAddress,
    std::uint64_t tableSize,
    std::uint64_t stringOffset) const {
    const auto hostTableSize =
        CheckedSize(tableSize, "ELF string table is too large");
    const auto hostStringOffset =
        CheckedSize(stringOffset, "ELF string-table offset is too large");

    if (hostStringOffset >= hostTableSize) {
        throw std::runtime_error(
            "ELF string-table offset is outside the string table");
    }

    const auto table = ResolveRange(tableAddress, hostTableSize);

    std::size_t end = hostStringOffset;
    while (end < table.size() && table[end] != std::byte{0}) {
        ++end;
    }

    if (end == table.size()) {
        throw std::runtime_error(
            "ELF string is not null-terminated inside the string table");
    }

    return std::string(
        reinterpret_cast<const char*>(table.data() + hostStringOffset),
        end - hostStringOffset);
}

} // namespace ps5emu::elf
