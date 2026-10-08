#include <ps5emu/runtime/GuestThreadMemory.hpp>

#include <limits>
#include <stdexcept>
#include <utility>

namespace ps5emu::runtime {
namespace {

std::uint64_t CheckedEnd(std::uint64_t start,
                         std::size_t size,
                         const char* message) {
    if (static_cast<std::uint64_t>(size) >
        std::numeric_limits<std::uint64_t>::max() - start) {
        throw std::runtime_error(message);
    }

    return start + static_cast<std::uint64_t>(size);
}

std::size_t CheckedSize(std::uint64_t size,
                        const char* message) {
    if (size > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error(message);
    }

    return static_cast<std::size_t>(size);
}

} // namespace

GuestThreadMemoryLayout GuestThreadMemory::Create(
    std::span<const std::byte> executableBytes,
    const elf::Image& image,
    memory::GuestMemory& memory,
    const GuestThreadMemoryOptions& options) {
    if (options.stackSize < 16) {
        throw std::invalid_argument(
            "Guest stack size must be at least 16 bytes");
    }

    const auto stackEnd =
        CheckedEnd(
            options.stackAddress,
            options.stackSize,
            "Guest stack address range overflows");

    const auto initialStackPointer =
        stackEnd & ~std::uint64_t{0x0f};

    if (initialStackPointer < options.stackAddress) {
        throw std::runtime_error(
            "Guest stack cannot provide a 16-byte aligned stack pointer");
    }

    auto stagedMemory = memory;

    stagedMemory.Map(
        options.stackAddress,
        options.stackSize,
        memory::Protection::Read |
            memory::Protection::Write);

    GuestThreadMemoryLayout layout;
    layout.stackAddress = options.stackAddress;
    layout.stackSize = options.stackSize;
    layout.initialStackPointer = initialStackPointer;

    if (image.tlsSegment.has_value() &&
        image.tlsSegment->memorySize != 0) {
        if (!options.tlsAddress.has_value()) {
            throw std::invalid_argument(
                "Guest TLS address is required for an ELF with PT_TLS");
        }

        const auto& tls = *image.tlsSegment;
        const auto tlsSize =
            CheckedSize(
                tls.memorySize,
                "Guest TLS block is too large for this host");
        const auto tlsFileSize =
            CheckedSize(
                tls.fileSize,
                "Guest TLS template is too large for this host");
        const auto tlsFileOffset =
            CheckedSize(
                tls.fileOffset,
                "Guest TLS file offset is too large for this host");

        if (tls.alignment > 1 &&
            (*options.tlsAddress % tls.alignment) != 0) {
            throw std::invalid_argument(
                "Guest TLS address does not satisfy PT_TLS alignment");
        }

        if (tlsFileOffset > executableBytes.size() ||
            tlsFileSize >
                executableBytes.size() - tlsFileOffset) {
            throw std::runtime_error(
                "Guest TLS template extends past end of executable");
        }

        stagedMemory.Map(
            *options.tlsAddress,
            tlsSize,
            memory::Protection::Read |
                memory::Protection::Write);

        if (tlsFileSize != 0) {
            stagedMemory.Initialize(
                *options.tlsAddress,
                executableBytes.subspan(
                    tlsFileOffset,
                    tlsFileSize));
        }

        layout.tlsAddress = options.tlsAddress;
        layout.tlsSize = tlsSize;
        layout.tlsAlignment = tls.alignment;
    }

    memory = std::move(stagedMemory);
    return layout;
}

} // namespace ps5emu::runtime
