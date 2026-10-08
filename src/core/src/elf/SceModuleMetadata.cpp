#include <ps5emu/elf/SceModuleMetadata.hpp>

#include <ps5emu/elf/ElfFileView.hpp>

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace ps5emu::elf {
namespace {

constexpr std::int64_t kTagNull = 0;

constexpr std::int64_t kSceModuleInfoLegacy = 0x6100000d;
constexpr std::int64_t kSceNeededModuleLegacy = 0x6100000f;
constexpr std::int64_t kSceExportLibraryLegacy = 0x61000013;
constexpr std::int64_t kSceImportLibraryLegacy = 0x61000015;

constexpr std::int64_t kSceModuleInfo = 0x61000043;
constexpr std::int64_t kSceNeededModule = 0x61000045;
constexpr std::int64_t kSceExportLibrary = 0x61000047;
constexpr std::int64_t kSceImportLibrary = 0x61000049;

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
            "SCE dynamic metadata extends past end of file");
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

bool IsModuleInfoTag(std::int64_t tag) {
    return tag == kSceModuleInfoLegacy || tag == kSceModuleInfo;
}

bool IsNeededModuleTag(std::int64_t tag) {
    return tag == kSceNeededModuleLegacy || tag == kSceNeededModule;
}

bool IsExportLibraryTag(std::int64_t tag) {
    return tag == kSceExportLibraryLegacy || tag == kSceExportLibrary;
}

bool IsImportLibraryTag(std::int64_t tag) {
    return tag == kSceImportLibraryLegacy || tag == kSceImportLibrary;
}

bool IsMetadataTag(std::int64_t tag) {
    return IsModuleInfoTag(tag) ||
           IsNeededModuleTag(tag) ||
           IsExportLibraryTag(tag) ||
           IsImportLibraryTag(tag);
}

void ValidateName(std::string_view name) {
    if (name.empty()) {
        throw std::runtime_error("SCE module metadata contains an empty name");
    }

    if (name.find_first_of("/\\:$\r\n") != std::string_view::npos) {
        throw std::runtime_error("SCE module metadata contains an invalid name");
    }
}

SceModuleRecord DecodeModuleRecord(
    std::uint64_t value,
    const ElfFileView& view,
    const DynamicMetadata& dynamic) {
    const auto nameOffset =
        static_cast<std::uint32_t>(value & 0xffffffffu);

    auto name = view.ReadString(
        *dynamic.stringTable,
        dynamic.stringTableSize,
        nameOffset);
    ValidateName(name);

    return SceModuleRecord{
        .id = static_cast<std::uint16_t>(value >> 48),
        .versionMajor =
            static_cast<std::uint8_t>((value >> 40) & 0xffu),
        .versionMinor =
            static_cast<std::uint8_t>((value >> 32) & 0xffu),
        .name = std::move(name),
    };
}

SceLibraryRecord DecodeLibraryRecord(
    std::uint64_t value,
    const ElfFileView& view,
    const DynamicMetadata& dynamic) {
    const auto nameOffset =
        static_cast<std::uint32_t>(value & 0xffffffffu);

    auto name = view.ReadString(
        *dynamic.stringTable,
        dynamic.stringTableSize,
        nameOffset);
    ValidateName(name);

    return SceLibraryRecord{
        .id = static_cast<std::uint16_t>(value >> 48),
        .version =
            static_cast<std::uint16_t>((value >> 32) & 0xffffu),
        .name = std::move(name),
    };
}

template <typename Record>
void PushUnique(std::vector<Record>& records,
                std::unordered_set<std::uint16_t>& ids,
                Record record,
                const char* message) {
    if (!ids.insert(record.id).second) {
        throw std::runtime_error(message);
    }

    records.push_back(std::move(record));
}

} // namespace

const SceModuleRecord* SceModuleMetadata::FindNeededModule(
    std::uint16_t id) const noexcept {
    const auto found = std::find_if(
        neededModules.begin(),
        neededModules.end(),
        [id](const SceModuleRecord& record) {
            return record.id == id;
        });

    return found == neededModules.end() ? nullptr : &*found;
}

const SceLibraryRecord* SceModuleMetadata::FindImportLibrary(
    std::uint16_t id) const noexcept {
    const auto found = std::find_if(
        importLibraries.begin(),
        importLibraries.end(),
        [id](const SceLibraryRecord& record) {
            return record.id == id;
        });

    return found == importLibraries.end() ? nullptr : &*found;
}

SceModuleMetadata SceModuleMetadataParser::Parse(
    std::span<const std::byte> bytes,
    const Image& image,
    const DynamicMetadata& dynamic) {
    SceModuleMetadata metadata;

    if (!image.dynamicSegment.has_value()) {
        return metadata;
    }

    const auto& segment = *image.dynamicSegment;
    const auto offset = CheckedSize(
        segment.fileOffset,
        "SCE dynamic metadata offset is too large");
    const auto size = CheckedSize(
        segment.fileSize,
        "SCE dynamic metadata size is too large");

    if (size % sizeof(DynamicEntry) != 0) {
        throw std::runtime_error(
            "ELF dynamic segment size is not entry-aligned");
    }

    if (offset > bytes.size() || size > bytes.size() - offset) {
        throw std::runtime_error(
            "ELF dynamic segment extends past end of file");
    }

    bool hasMetadata = false;
    for (std::size_t position = 0;
         position < size;
         position += sizeof(DynamicEntry)) {
        const auto entry =
            ReadObject<DynamicEntry>(bytes, offset + position);

        if (entry.tag == kTagNull) {
            break;
        }

        if (IsMetadataTag(entry.tag)) {
            hasMetadata = true;
            break;
        }
    }

    if (!hasMetadata) {
        return metadata;
    }

    if (!dynamic.stringTable.has_value() ||
        dynamic.stringTableSize == 0) {
        throw std::runtime_error(
            "SCE module metadata requires a dynamic string table");
    }

    const ElfFileView view(bytes, image);
    std::unordered_set<std::uint16_t> neededModuleIds;
    std::unordered_set<std::uint16_t> exportLibraryIds;
    std::unordered_set<std::uint16_t> importLibraryIds;
    bool terminated = false;

    for (std::size_t position = 0;
         position < size;
         position += sizeof(DynamicEntry)) {
        const auto entry =
            ReadObject<DynamicEntry>(bytes, offset + position);

        if (entry.tag == kTagNull) {
            terminated = true;
            break;
        }

        if (IsModuleInfoTag(entry.tag)) {
            if (metadata.module.has_value()) {
                throw std::runtime_error(
                    "ELF contains duplicate SCE module information");
            }

            metadata.module =
                DecodeModuleRecord(entry.value, view, dynamic);
            continue;
        }

        if (IsNeededModuleTag(entry.tag)) {
            PushUnique(
                metadata.neededModules,
                neededModuleIds,
                DecodeModuleRecord(entry.value, view, dynamic),
                "ELF contains duplicate SCE needed-module IDs");
            continue;
        }

        if (IsExportLibraryTag(entry.tag)) {
            PushUnique(
                metadata.exportLibraries,
                exportLibraryIds,
                DecodeLibraryRecord(entry.value, view, dynamic),
                "ELF contains duplicate SCE export-library IDs");
            continue;
        }

        if (IsImportLibraryTag(entry.tag)) {
            PushUnique(
                metadata.importLibraries,
                importLibraryIds,
                DecodeLibraryRecord(entry.value, view, dynamic),
                "ELF contains duplicate SCE import-library IDs");
        }
    }

    if (!terminated && size != 0) {
        throw std::runtime_error(
            "ELF dynamic segment does not contain DT_NULL");
    }

    return metadata;
}

} // namespace ps5emu::elf
