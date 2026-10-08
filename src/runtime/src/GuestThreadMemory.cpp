#include <ps5emu/runtime/GuestThreadMemory.hpp>

#include <ps5emu/runtime/GuestCallDispatcher.hpp>

#include <array>
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ps5emu::runtime {
namespace {

constexpr std::size_t kThreadControlBlockSize = 0x30;
constexpr std::size_t kSelfPointerOffset = 0x00;
constexpr std::size_t kStackGuardOffset = 0x28;
constexpr std::uint64_t kMinimumTlsAlignment = 16;

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

std::size_t AlignUp(std::size_t value,
                    std::uint64_t alignment) {
    if (alignment <= 1) {
        return value;
    }

    const auto hostAlignment =
        CheckedSize(alignment, "Guest TLS alignment is too large for this host");
    const auto mask = hostAlignment - 1;

    if (value > std::numeric_limits<std::size_t>::max() - mask) {
        throw std::runtime_error("Guest TLS block size overflows");
    }

    return (value + mask) & ~mask;
}

void InitializeU64(memory::GuestMemory& memory,
                   std::uint64_t address,
                   std::uint64_t value) {
    std::array<std::byte, sizeof(value)> bytes{};
    std::memcpy(bytes.data(), &value, sizeof(value));
    memory.Initialize(address, bytes);
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

        const auto tlsAlignment =
            std::max<std::uint64_t>(
                tls.alignment,
                kMinimumTlsAlignment);

        if ((*options.tlsAddress % tlsAlignment) != 0) {
            throw std::invalid_argument(
                "Guest TLS address does not satisfy runtime TLS alignment");
        }

        if (tlsFileOffset > executableBytes.size() ||
            tlsFileSize >
                executableBytes.size() - tlsFileOffset) {
            throw std::runtime_error(
                "Guest TLS template extends past end of executable");
        }

        const auto tlsBlockSize =
            AlignUp(tlsSize, tlsAlignment);

        if (tlsBlockSize >
            std::numeric_limits<std::size_t>::max() -
                kThreadControlBlockSize) {
            throw std::runtime_error(
                "Guest TLS runtime allocation size overflows");
        }

        const auto tlsRuntimeSize =
            tlsBlockSize + kThreadControlBlockSize;

        const auto threadPointer =
            CheckedEnd(
                *options.tlsAddress,
                tlsBlockSize,
                "Guest TLS thread pointer overflows");

        CheckedEnd(
            threadPointer,
            kThreadControlBlockSize,
            "Guest TLS thread-control block overflows");

        stagedMemory.Map(
            *options.tlsAddress,
            tlsRuntimeSize,
            memory::Protection::Read |
                memory::Protection::Write);

        if (tlsFileSize != 0) {
            stagedMemory.Initialize(
                *options.tlsAddress,
                executableBytes.subspan(
                    tlsFileOffset,
                    tlsFileSize));
        }

        InitializeU64(
            stagedMemory,
            threadPointer + kSelfPointerOffset,
            threadPointer);
        InitializeU64(
            stagedMemory,
            threadPointer + kStackGuardOffset,
            options.stackGuard);

        layout.tlsAddress = options.tlsAddress;
        layout.tlsSize = tlsSize;
        layout.tlsBlockSize = tlsBlockSize;
        layout.tlsAlignment = tlsAlignment;
        layout.threadPointer = threadPointer;
        layout.threadControlBlockSize =
            kThreadControlBlockSize;
    }

    memory = std::move(stagedMemory);
    return layout;
}

void GuestThreadMemory::ApplyToContext(
    const GuestThreadMemoryLayout& layout,
    SysvGuestContext& context) {
    context.rsp = layout.initialStackPointer;
    context.fsBase = layout.threadPointer.value_or(0);
}

} // namespace ps5emu::runtime
