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

std::size_t CheckedFileOffset(std::span<const std::byte> bytes,
                              std::uint64_t base,
                              std::uint64_t delta,
                              std::size_t requiredSize,
                              const char* message) {
    if (base > std::numeric_limits<std::uint64_t>::max() - delta) {
        throw std::runtime_error(message);
    }

    const auto offset = CheckedSize(
        base + delta,
        "ELF file offset is too large for this host");

    if (offset > bytes.size() ||
        requiredSize > bytes.size() - offset) {
        throw std::runtime_error(message);
    }

    return offset;
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

        return CheckedFileOffset(
            bytes_,
            segment.fileOffset,
            delta,
            requiredSize,
            "ELF virtual address resolves outside the input file");
    }

    throw std::runtime_error(
        "ELF virtual address is not backed by a load segment");
}

std::size_t ElfFileView::ResolveFileOffset(
    const DynamicTableReference& reference,
    std::size_t requiredSize) const {
    if (reference.kind == DynamicReferenceKind::VirtualAddress) {
        return ResolveFileOffset(reference.value, requiredSize);
    }

    if (!image_.sceDynamicDataSegment.has_value()) {
        throw std::runtime_error(
            "SCE dynamic table reference requires PT_SCE_DYNLIBDATA");
    }

    const auto& segment = *image_.sceDynamicDataSegment;
    if (reference.value > segment.fileSize) {
        throw std::runtime_error(
            "SCE dynamic table exceeds PT_SCE_DYNLIBDATA");
    }

    const auto available =
        CheckedSize(
            segment.fileSize - reference.value,
            "SCE dynamic data range is too large for this host");

    if (requiredSize > available) {
        throw std::runtime_error(
            "SCE dynamic table exceeds PT_SCE_DYNLIBDATA");
    }

    return CheckedFileOffset(
        bytes_,
        segment.fileOffset,
        reference.value,
        requiredSize,
        "SCE dynamic table resolves outside the input file");
}

std::span<const std::byte> ElfFileView::ResolveRange(
    std::uint64_t virtualAddress,
    std::size_t size) const {
    const auto offset = ResolveFileOffset(virtualAddress, size);
    return bytes_.subspan(offset, size);
}

std::span<const std::byte> ElfFileView::ResolveRange(
    const DynamicTableReference& reference,
    std::size_t size) const {
    const auto offset = ResolveFileOffset(reference, size);
    return bytes_.subspan(offset, size);
}

std::string ElfFileView::ReadString(
    const DynamicTableReference& tableReference,
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

    const auto table = ResolveRange(tableReference, hostTableSize);

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
