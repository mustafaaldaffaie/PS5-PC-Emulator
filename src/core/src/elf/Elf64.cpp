#include <ps5emu/elf/Elf64.hpp>

#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <type_traits>

namespace ps5emu::elf {
namespace {

constexpr std::array<std::byte, 4> kElfMagic{
    std::byte{0x7f}, std::byte{'E'}, std::byte{'L'}, std::byte{'F'}
};

constexpr std::uint8_t kElfClass64 = 2;
constexpr std::uint8_t kElfDataLittleEndian = 1;
constexpr std::uint32_t kProgramTypeLoad = 1;

template <typename T>
T ReadObject(std::span<const std::byte> bytes, std::size_t offset) {
    static_assert(std::is_trivially_copyable_v<T>);

    if (offset > bytes.size() || bytes.size() - offset < sizeof(T)) {
        throw std::runtime_error("ELF structure extends past end of file");
    }

    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

#pragma pack(push, 1)
struct ElfHeader {
    std::uint8_t ident[16];
    std::uint16_t type;
    std::uint16_t machine;
    std::uint32_t version;
    std::uint64_t entry;
    std::uint64_t programHeaderOffset;
    std::uint64_t sectionHeaderOffset;
    std::uint32_t flags;
    std::uint16_t headerSize;
    std::uint16_t programHeaderEntrySize;
    std::uint16_t programHeaderCount;
    std::uint16_t sectionHeaderEntrySize;
    std::uint16_t sectionHeaderCount;
    std::uint16_t sectionHeaderStringIndex;
};

struct ProgramHeader {
    std::uint32_t type;
    std::uint32_t flags;
    std::uint64_t offset;
    std::uint64_t virtualAddress;
    std::uint64_t physicalAddress;
    std::uint64_t fileSize;
    std::uint64_t memorySize;
    std::uint64_t alignment;
};
#pragma pack(pop)

} // namespace

Image Elf64::Parse(std::span<const std::byte> bytes) {
    if (bytes.size() < sizeof(ElfHeader)) {
        throw std::runtime_error("ELF file is too small");
    }

    for (std::size_t i = 0; i < kElfMagic.size(); ++i) {
        if (bytes[i] != kElfMagic[i]) {
            throw std::runtime_error("Input is not an ELF file");
        }
    }

    const auto header = ReadObject<ElfHeader>(bytes, 0);

    if (header.ident[4] != kElfClass64) {
        throw std::runtime_error("Only ELF64 executables are supported");
    }

    if (header.ident[5] != kElfDataLittleEndian) {
        throw std::runtime_error("Only little-endian ELF executables are supported");
    }

    if (header.programHeaderEntrySize != sizeof(ProgramHeader)) {
        throw std::runtime_error("Unexpected ELF64 program header size");
    }

    Image image;
    image.entryPoint = header.entry;

    for (std::uint16_t index = 0; index < header.programHeaderCount; ++index) {
        const std::uint64_t offset =
            header.programHeaderOffset +
            static_cast<std::uint64_t>(index) * header.programHeaderEntrySize;

        if (offset > bytes.size()) {
            throw std::runtime_error("ELF program header offset is invalid");
        }

        const auto programHeader =
            ReadObject<ProgramHeader>(bytes, static_cast<std::size_t>(offset));

        if (programHeader.type != kProgramTypeLoad) {
            continue;
        }

        if (programHeader.fileSize > programHeader.memorySize) {
            throw std::runtime_error("ELF load segment file size exceeds memory size");
        }

        if (programHeader.offset > bytes.size() ||
            programHeader.fileSize > bytes.size() - programHeader.offset) {
            throw std::runtime_error("ELF load segment extends past end of file");
        }

        image.loadSegments.push_back(Segment{
            .virtualAddress = programHeader.virtualAddress,
            .memorySize = programHeader.memorySize,
            .fileSize = programHeader.fileSize,
            .fileOffset = programHeader.offset,
            .flags = programHeader.flags,
            .alignment = programHeader.alignment,
        });
    }

    return image;
}

Image Elf64::ParseFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open ELF file");
    }

    const auto size = file.tellg();
    if (size < 0) {
        throw std::runtime_error("Failed to determine ELF file size");
    }

    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);

    if (!bytes.empty()) {
        file.read(reinterpret_cast<char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        if (!file) {
            throw std::runtime_error("Failed to read ELF file");
        }
    }

    return Parse(bytes);
}

} // namespace ps5emu::elf
