#include <ps5emu/elf/DynamicMetadata.hpp>

#include <ps5emu/elf/ElfFileView.hpp>

#include <cstring>
#include <limits>
#include <optional>
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
constexpr std::int64_t kTagPltRelocationType = 20;
constexpr std::int64_t kTagJumpRelocation = 23;

constexpr std::int64_t kSceTagJumpRelocation = 0x61000029;
constexpr std::int64_t kSceTagPltRelocationType = 0x6100002b;
constexpr std::int64_t kSceTagPltRelocationSize = 0x6100002d;
constexpr std::int64_t kSceTagRela = 0x6100002f;
constexpr std::int64_t kSceTagRelaSize = 0x61000031;
constexpr std::int64_t kSceTagRelaEntrySize = 0x61000033;
constexpr std::int64_t kSceTagStringTable = 0x61000035;
constexpr std::int64_t kSceTagStringTableSize = 0x61000037;
constexpr std::int64_t kSceTagSymbolTable = 0x61000039;
constexpr std::int64_t kSceTagSymbolEntrySize = 0x6100003b;
constexpr std::int64_t kSceTagSymbolTableSize = 0x6100003f;

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

void AssignReference(
    std::optional<DynamicTableReference>& target,
    std::uint64_t value,
    DynamicReferenceKind kind,
    const char* message) {
    if (target.has_value()) {
        throw std::runtime_error(message);
    }

    target = DynamicTableReference{
        .value = value,
        .kind = kind,
    };
}

void AssignScalar(std::optional<std::uint64_t>& target,
                  std::uint64_t value,
                  const char* message) {
    if (target.has_value()) {
        throw std::runtime_error(message);
    }

    target = value;
}

bool UsesSceReference(const DynamicMetadata& metadata) {
    const auto isSce = [](const auto& reference) {
        return reference.has_value() &&
               reference->kind ==
                   DynamicReferenceKind::SceDynamicDataOffset;
    };

    return isSce(metadata.stringTable) ||
           isSce(metadata.symbolTable) ||
           isSce(metadata.relaTable) ||
           isSce(metadata.jumpRelocationTable);
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
    std::optional<std::uint64_t> stringTableSize;
    std::optional<std::uint64_t> symbolTableSize;
    std::optional<std::uint64_t> symbolEntrySize;
    std::optional<std::uint64_t> relaSize;
    std::optional<std::uint64_t> relaEntrySize;
    std::optional<std::uint64_t> jumpRelocationSize;
    std::optional<std::uint64_t> pltRelocationType;
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
            AssignReference(
                metadata.stringTable,
                entry.value,
                DynamicReferenceKind::VirtualAddress,
                "Duplicate or ambiguous dynamic string table");
            break;

        case kSceTagStringTable:
            AssignReference(
                metadata.stringTable,
                entry.value,
                DynamicReferenceKind::SceDynamicDataOffset,
                "Duplicate or ambiguous dynamic string table");
            break;

        case kTagStringTableSize:
        case kSceTagStringTableSize:
            AssignScalar(
                stringTableSize,
                entry.value,
                "Duplicate or ambiguous dynamic string-table size");
            break;

        case kTagSymbolTable:
            AssignReference(
                metadata.symbolTable,
                entry.value,
                DynamicReferenceKind::VirtualAddress,
                "Duplicate or ambiguous dynamic symbol table");
            break;

        case kSceTagSymbolTable:
            AssignReference(
                metadata.symbolTable,
                entry.value,
                DynamicReferenceKind::SceDynamicDataOffset,
                "Duplicate or ambiguous dynamic symbol table");
            break;

        case kSceTagSymbolTableSize:
            AssignScalar(
                symbolTableSize,
                entry.value,
                "Duplicate dynamic symbol-table size");
            break;

        case kTagSymbolEntrySize:
        case kSceTagSymbolEntrySize:
            AssignScalar(
                symbolEntrySize,
                entry.value,
                "Duplicate or ambiguous dynamic symbol-entry size");
            break;

        case kTagRela:
            AssignReference(
                metadata.relaTable,
                entry.value,
                DynamicReferenceKind::VirtualAddress,
                "Duplicate or ambiguous RELA table");
            break;

        case kSceTagRela:
            AssignReference(
                metadata.relaTable,
                entry.value,
                DynamicReferenceKind::SceDynamicDataOffset,
                "Duplicate or ambiguous RELA table");
            break;

        case kTagRelaSize:
        case kSceTagRelaSize:
            AssignScalar(
                relaSize,
                entry.value,
                "Duplicate or ambiguous RELA table size");
            break;

        case kTagRelaEntrySize:
        case kSceTagRelaEntrySize:
            AssignScalar(
                relaEntrySize,
                entry.value,
                "Duplicate or ambiguous RELA entry size");
            break;

        case kTagJumpRelocation:
            AssignReference(
                metadata.jumpRelocationTable,
                entry.value,
                DynamicReferenceKind::VirtualAddress,
                "Duplicate or ambiguous PLT relocation table");
            break;

        case kSceTagJumpRelocation:
            AssignReference(
                metadata.jumpRelocationTable,
                entry.value,
                DynamicReferenceKind::SceDynamicDataOffset,
                "Duplicate or ambiguous PLT relocation table");
            break;

        case kTagPltRelocationSize:
        case kSceTagPltRelocationSize:
            AssignScalar(
                jumpRelocationSize,
                entry.value,
                "Duplicate or ambiguous PLT relocation size");
            break;

        case kTagPltRelocationType:
        case kSceTagPltRelocationType:
            AssignScalar(
                pltRelocationType,
                entry.value,
                "Duplicate or ambiguous PLT relocation type");
            break;

        default:
            break;
        }
    }

    if (!foundNull && dynamicSize != 0) {
        throw std::runtime_error(
            "ELF dynamic segment does not contain DT_NULL");
    }

    metadata.stringTableSize = stringTableSize.value_or(0);
    metadata.symbolTableSize = symbolTableSize.value_or(0);
    metadata.symbolEntrySize = symbolEntrySize.value_or(0);
    metadata.relaSize = relaSize.value_or(0);
    metadata.relaEntrySize = relaEntrySize.value_or(0);
    metadata.jumpRelocationSize = jumpRelocationSize.value_or(0);
    metadata.pltRelocationType = pltRelocationType;

    if (UsesSceReference(metadata) &&
        !image.sceDynamicDataSegment.has_value()) {
        throw std::runtime_error(
            "SCE dynamic tables require PT_SCE_DYNLIBDATA");
    }

    if (neededOffsets.empty()) {
        return metadata;
    }

    if (!metadata.stringTable.has_value() ||
        metadata.stringTableSize == 0) {
        throw std::runtime_error(
            "ELF DT_NEEDED entries require a dynamic string table");
    }

    const ElfFileView view(bytes, image);
    metadata.neededLibraries.reserve(neededOffsets.size());

    for (const auto offset : neededOffsets) {
        metadata.neededLibraries.push_back(
            view.ReadString(
                *metadata.stringTable,
                metadata.stringTableSize,
                offset));
    }

    return metadata;
}

} // namespace ps5emu::elf
