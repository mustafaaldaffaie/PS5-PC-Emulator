#include <ps5emu/loader/ExecutableImageLoader.hpp>

#include <ps5emu/elf/Elf64.hpp>

#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace ps5emu::loader {
namespace {

constexpr std::uint32_t kElfFlagExecute = 1;
constexpr std::uint32_t kElfFlagWrite = 2;
constexpr std::uint32_t kElfFlagRead = 4;

memory::Protection ConvertProtection(std::uint32_t flags) {
    auto protection = memory::Protection::None;

    if ((flags & kElfFlagRead) != 0) {
        protection = protection | memory::Protection::Read;
    }

    if ((flags & kElfFlagWrite) != 0) {
        protection = protection | memory::Protection::Write;
    }

    if ((flags & kElfFlagExecute) != 0) {
        protection = protection | memory::Protection::Execute;
    }

    return protection;
}

std::size_t CheckedSize(std::uint64_t value, const char* description) {
    if (value > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error(description);
    }

    return static_cast<std::size_t>(value);
}

bool EntryPointIsExecutable(const elf::Image& image) {
    for (const auto& segment : image.loadSegments) {
        if ((segment.flags & kElfFlagExecute) == 0 ||
            segment.memorySize == 0 ||
            image.entryPoint < segment.virtualAddress) {
            continue;
        }

        const auto offset = image.entryPoint - segment.virtualAddress;
        if (offset < segment.memorySize) {
            return true;
        }
    }

    return false;
}

std::vector<std::byte> ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open executable file");
    }

    const auto size = file.tellg();
    if (size < 0) {
        throw std::runtime_error("Failed to determine executable file size");
    }

    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);

    if (!bytes.empty()) {
        file.read(reinterpret_cast<char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        if (!file) {
            throw std::runtime_error("Failed to read executable file");
        }
    }

    return bytes;
}

} // namespace

LoadedImage ExecutableImageLoader::LoadElf(
    std::span<const std::byte> bytes,
    memory::GuestMemory& memory) {
    const auto image = elf::Elf64::Parse(bytes);

    if (!EntryPointIsExecutable(image)) {
        throw std::runtime_error(
            "ELF entry point is not inside an executable load segment");
    }

    std::size_t mappedSegmentCount = 0;

    for (const auto& segment : image.loadSegments) {
        if (segment.memorySize == 0) {
            continue;
        }

        const auto memorySize =
            CheckedSize(segment.memorySize,
                        "ELF load segment is too large for this host");
        const auto fileSize =
            CheckedSize(segment.fileSize,
                        "ELF load segment file size is too large for this host");
        const auto fileOffset =
            CheckedSize(segment.fileOffset,
                        "ELF load segment offset is too large for this host");

        memory.Map(segment.virtualAddress,
                   memorySize,
                   ConvertProtection(segment.flags));

        if (fileSize != 0) {
            memory.Initialize(
                segment.virtualAddress,
                bytes.subspan(fileOffset, fileSize));
        }

        ++mappedSegmentCount;
    }

    return LoadedImage{
        .entryPoint = image.entryPoint,
        .mappedSegmentCount = mappedSegmentCount,
    };
}

LoadedImage ExecutableImageLoader::LoadElfFile(
    const std::filesystem::path& path,
    memory::GuestMemory& memory) {
    const auto bytes = ReadFile(path);
    return LoadElf(bytes, memory);
}

} // namespace ps5emu::loader
