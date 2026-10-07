#include <ps5emu/elf/DynamicMetadata.hpp>

#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace ps5emu::elf {
namespace {

constexpr std::int64_t kTagNull = 0;
constexpr std::int64_t kTagNeeded = 1;
constexpr std::int64_t kTagPltRelocationSize = 2;
constexpr std::int64_t kTagStringTable = 5;
constexpr std::int64_t kTagSymbolTable = 6;
constexpr std::int64_t kTagRela = 7;
constexpr std::int64_t kTagRelaSize = 8;
constexpr std::int64_t kTagRelaEntrySize = 9;
constexpr std::int64_t kTagStringTableSize = 10;
constexpr std::int64_t kTagSymbolEntrySize = 11;
constexpr std::int64_t kTagJumpRelocation = 23;

#pragma pack(push, 1)
struct DynamicEntry {
    std::int64_t tag;
    std::uint64_t value;
};
#pragma pack(pop)

static_assert(sizeof(DynamicEntry) == 16);

template <typename T>
T ReadObject(std::span<const std::byte> bytes, std::size_t offset) {
    static_assert(std::is_trivially_copyable_v<T>);

    if (offset > bytes.size() || bytes.size() - offset < sizeof(T)) {
        throw std::runtime_error(
            "ELF dynamic structure extends past end of file");
    }

    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

std::size_t CheckedSize(std::uint64_t value, const char* message) {
    if (value > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error(message);
    }

    return static_cast<std::size_t>(value);
}

std::size_t VirtualAddressToFileOffset(
    std::span<const std::byte> bytes,
    const Image& image,
    std::uint64_t virtualAddress,
    std::size_t requiredSize) {
    for (const auto& segment : image.loadSegments) {
        if (virtualAddress < segment.virtualAddress) {
            continue;
        }

        const auto delta = virtualAddress - segment.virtualAddress;
        if (delta > segment.fileSize) {
            continue;
        }

        if (requiredSize >
            CheckedSize(segment.fileSize - delta,
                        "ELF file-backed segment range is too large")) {
            continue;
        }

        if (segment.fileOffset >
            std::numeric_limits<std::uint64_t>::max() - delta) {
            throw std::runtime_error("ELF file offset overflows");
        }

        const auto fileOffset = segment.fileOffset + delta;
        const auto hostOffset =
            CheckedSize(fileOffset, "ELF file offset is too large for this host");

        if (hostOffset > bytes.size() ||
            requiredSize > bytes.size() - hostOffset) {
            throw std::runtime_error(
                "ELF virtual address resolves outside the input file");
        }

        return hostOffset;
    }

    throw std::runtime_error(
        "ELF virtual address is not backed by a load segment");
}

std::string ReadString(std::span<const std::byte> stringTable,
                       std::uint64_t offset) {
    const auto hostOffset =
        CheckedSize(offset, "ELF string-table offset is too large");

    if (hostOffset >= stringTable.size()) {
        throw std::runtime_error(
            "ELF string-table offset is outside the string table");
    }

    std::size_t end = hostOffset;
    while (end < stringTable.size() &&
           stringTable[end] != std::byte{0}) {
        ++end;
    }

    if (end == stringTable.size()) {
        throw std::runtime_error(
            "ELF string is not null-terminated inside the string table");
    }

    return std::string(
        reinterpret_cast<const char*>(stringTable.data() + hostOffset),
        end - hostOffset);
}

} // namespace

DynamicMetadata DynamicMetadataParser::Parse(
    std::span<const std::byte> bytes,
    const Image& image) {
    DynamicMetadata metadata;

    if (!image.dynamicSegment.has_value()) {
        return metadata;
    }

    const auto& dynamic = *image.dynamicSegment;
    const auto dynamicOffset =
        CheckedSize(dynamic.fileOffset,
                    "ELF dynamic segment offset is too large");
    const auto dynamicSize =
        CheckedSize(dynamic.fileSize,
                    "ELF dynamic segment size is too large");

    if (dynamicSize % sizeof(DynamicEntry) != 0) {
        throw std::runtime_error(
            "ELF dynamic segment size is not entry-aligned");
    }

    if (dynamicOffset > bytes.size() ||
        dynamicSize > bytes.size() - dynamicOffset) {
        throw std::runtime_error(
            "ELF dynamic segment extends past end of file");
    }

    std::vector<std::uint64_t> neededOffsets;
    bool foundNull = false;

    for (std::size_t offset = 0;
         offset < dynamicSize;
         offset += sizeof(DynamicEntry)) {
        const auto entry =
            ReadObject<DynamicEntry>(bytes, dynamicOffset + offset);

        if (entry.tag == kTagNull) {
            foundNull = true;
            break;
        }

        switch (entry.tag) {
        case kTagNeeded:
            neededOffsets.push_back(entry.value);
            break;
        case kTagStringTable:
            metadata.stringTableAddress = entry.value;
            break;
        case kTagStringTableSize:
            metadata.stringTableSize = entry.value;
            break;
        case kTagSymbolTable:
            metadata.symbolTableAddress = entry.value;
            break;
        case kTagSymbolEntrySize:
            metadata.symbolEntrySize = entry.value;
            break;
        case kTagRela:
            metadata.relaAddress = entry.value;
            break;
        case kTagRelaSize:
            metadata.relaSize = entry.value;
            break;
        case kTagRelaEntrySize:
            metadata.relaEntrySize = entry.value;
            break;
        case kTagJumpRelocation:
            metadata.jumpRelocationAddress = entry.value;
            break;
        case kTagPltRelocationSize:
            metadata.jumpRelocationSize = entry.value;
            break;
        default:
            break;
        }
    }

    if (!foundNull && dynamicSize != 0) {
        throw std::runtime_error(
            "ELF dynamic segment does not contain DT_NULL");
    }

    if (neededOffsets.empty()) {
        return metadata;
    }

    if (!metadata.stringTableAddress.has_value() ||
        metadata.stringTableSize == 0) {
        throw std::runtime_error(
            "ELF DT_NEEDED entries require a dynamic string table");
    }

    const auto stringTableSize =
        CheckedSize(metadata.stringTableSize,
                    "ELF dynamic string table is too large");
    const auto stringTableOffset =
        VirtualAddressToFileOffset(
            bytes,
            image,
            *metadata.stringTableAddress,
            stringTableSize);

    const auto stringTable =
        bytes.subspan(stringTableOffset, stringTableSize);

    metadata.neededLibraries.reserve(neededOffsets.size());
    for (const auto offset : neededOffsets) {
        metadata.neededLibraries.push_back(
            ReadString(stringTable, offset));
    }

    return metadata;
}

} // namespace ps5emu::elf
