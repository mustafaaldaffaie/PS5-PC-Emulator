#include <ps5emu/runtime/NativeMemoryRegion.hpp>

#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#elif defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#else
#error "NativeMemoryRegion currently supports Windows and Linux only"
#endif

namespace ps5emu::runtime {
namespace {

std::size_t PageSize() {
#if defined(_WIN32)
    SYSTEM_INFO information{};
    GetSystemInfo(&information);

    if (information.dwPageSize == 0) {
        throw std::runtime_error(
            "Windows reported a zero virtual-memory page size");
    }

    return static_cast<std::size_t>(information.dwPageSize);
#elif defined(__linux__)
    const auto value = sysconf(_SC_PAGESIZE);
    if (value <= 0) {
        throw std::runtime_error(
            "Linux failed to report the virtual-memory page size");
    }

    return static_cast<std::size_t>(value);
#endif
}

std::size_t AllocationAlignment() {
#if defined(_WIN32)
    SYSTEM_INFO information{};
    GetSystemInfo(&information);

    if (information.dwAllocationGranularity == 0) {
        throw std::runtime_error(
            "Windows reported a zero allocation granularity");
    }

    return static_cast<std::size_t>(
        information.dwAllocationGranularity);
#elif defined(__linux__)
    return PageSize();
#endif
}

std::size_t RoundUp(std::size_t value,
                    std::size_t alignment) {
    const auto remainder = value % alignment;
    if (remainder == 0) {
        return value;
    }

    const auto extra = alignment - remainder;
    if (value > std::numeric_limits<std::size_t>::max() - extra) {
        throw std::overflow_error(
            "Native memory region size overflows");
    }

    return value + extra;
}

void ValidateProtection(memory::Protection protection) {
    const bool readable =
        memory::HasProtection(
            protection,
            memory::Protection::Read);
    const bool writable =
        memory::HasProtection(
            protection,
            memory::Protection::Write);
    const bool executable =
        memory::HasProtection(
            protection,
            memory::Protection::Execute);

    if (writable && !readable) {
        throw std::invalid_argument(
            "Native writable memory must also be readable");
    }

    if (writable && executable) {
        throw std::invalid_argument(
            "Native memory cannot be writable and executable simultaneously");
    }
}

#if defined(_WIN32)

DWORD WindowsProtection(memory::Protection protection) {
    const bool readable =
        memory::HasProtection(
            protection,
            memory::Protection::Read);
    const bool writable =
        memory::HasProtection(
            protection,
            memory::Protection::Write);
    const bool executable =
        memory::HasProtection(
            protection,
            memory::Protection::Execute);

    if (executable && readable) {
        return PAGE_EXECUTE_READ;
    }
    if (executable) {
        return PAGE_EXECUTE;
    }
    if (writable) {
        return PAGE_READWRITE;
    }
    if (readable) {
        return PAGE_READONLY;
    }
    return PAGE_NOACCESS;
}

#elif defined(__linux__)

int LinuxProtection(memory::Protection protection) {
    int result = PROT_NONE;

    if (memory::HasProtection(
            protection,
            memory::Protection::Read)) {
        result |= PROT_READ;
    }
    if (memory::HasProtection(
            protection,
            memory::Protection::Write)) {
        result |= PROT_WRITE;
    }
    if (memory::HasProtection(
            protection,
            memory::Protection::Execute)) {
        result |= PROT_EXEC;
    }

    return result;
}

#endif

void FlushCode(void* address,
               std::size_t size) noexcept {
#if defined(_WIN32)
    static_cast<void>(
        FlushInstructionCache(
            GetCurrentProcess(),
            address,
            size));
#elif defined(__linux__)
    auto* begin = static_cast<char*>(address);
    __builtin___clear_cache(begin, begin + size);
#endif
}

} // namespace

NativeMemoryRegion NativeMemoryRegion::Allocate(
    std::size_t size) {
    return AllocateImpl(0, size, false);
}

NativeMemoryRegion NativeMemoryRegion::AllocateAt(
    std::uintptr_t address,
    std::size_t size) {
    return AllocateImpl(address, size, true);
}

NativeMemoryRegion NativeMemoryRegion::AllocateImpl(
    std::uintptr_t requestedAddress,
    std::size_t size,
    bool fixed) {
    if (size == 0) {
        throw std::invalid_argument(
            "Native memory region size cannot be zero");
    }

    if (fixed) {
        if (requestedAddress == 0) {
            throw std::invalid_argument(
                "Fixed native memory address cannot be zero");
        }

        const auto alignment = AllocationAlignment();
        if ((requestedAddress % alignment) != 0) {
            throw std::invalid_argument(
                "Fixed native memory address is not allocation aligned");
        }
    }

    const auto mappedSize =
        RoundUp(size, PageSize());

    void* address = nullptr;
    void* requested = fixed
        ? reinterpret_cast<void*>(requestedAddress)
        : nullptr;

#if defined(_WIN32)
    address = VirtualAlloc(
        requested,
        mappedSize,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE);

    if (address == nullptr) {
        throw std::runtime_error(
            "VirtualAlloc failed for native memory region");
    }

    if (fixed && address != requested) {
        static_cast<void>(
            VirtualFree(address, 0, MEM_RELEASE));
        throw std::runtime_error(
            "VirtualAlloc did not honor the fixed native address");
    }
#elif defined(__linux__)
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#if defined(MAP_FIXED_NOREPLACE)
    if (fixed) {
        flags |= MAP_FIXED_NOREPLACE;
    }
#endif

    address = mmap(
        requested,
        mappedSize,
        PROT_READ | PROT_WRITE,
        flags,
        -1,
        0);

    if (address == MAP_FAILED) {
        throw std::runtime_error(
            "mmap failed for native memory region");
    }

    if (fixed && address != requested) {
        static_cast<void>(
            munmap(address, mappedSize));
        throw std::runtime_error(
            "mmap did not honor the fixed native address");
    }
#endif

    return NativeMemoryRegion(
        address,
        size,
        mappedSize,
        memory::Protection::Read |
            memory::Protection::Write);
}

NativeMemoryRegion::NativeMemoryRegion(
    void* address,
    std::size_t size,
    std::size_t mappedSize,
    memory::Protection protection) noexcept
    : address_(address),
      size_(size),
      mappedSize_(mappedSize),
      protection_(protection) {
}

NativeMemoryRegion::NativeMemoryRegion(
    NativeMemoryRegion&& other) noexcept
    : address_(std::exchange(other.address_, nullptr)),
      size_(std::exchange(other.size_, 0)),
      mappedSize_(std::exchange(other.mappedSize_, 0)),
      protection_(
          std::exchange(
              other.protection_,
              memory::Protection::None)) {
}

NativeMemoryRegion& NativeMemoryRegion::operator=(
    NativeMemoryRegion&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    Release();

    address_ =
        std::exchange(other.address_, nullptr);
    size_ =
        std::exchange(other.size_, 0);
    mappedSize_ =
        std::exchange(other.mappedSize_, 0);
    protection_ =
        std::exchange(
            other.protection_,
            memory::Protection::None);

    return *this;
}

NativeMemoryRegion::~NativeMemoryRegion() {
    Release();
}

void NativeMemoryRegion::Write(
    std::size_t offset,
    std::span<const std::byte> bytes) {
    if (!memory::HasProtection(
            protection_,
            memory::Protection::Write)) {
        throw std::runtime_error(
            "Native memory region is not writable");
    }

    if (offset > size_ ||
        bytes.size() > size_ - offset) {
        throw std::out_of_range(
            "Native memory write is outside the requested region");
    }

    if (bytes.empty()) {
        return;
    }

    std::memcpy(
        static_cast<std::byte*>(address_) + offset,
        bytes.data(),
        bytes.size());
}

void NativeMemoryRegion::Protect(
    memory::Protection protection) {
    if (address_ == nullptr) {
        throw std::runtime_error(
            "Cannot protect an empty native memory region");
    }

    ValidateProtection(protection);

#if defined(_WIN32)
    DWORD previousProtection = 0;
    if (!VirtualProtect(
            address_,
            mappedSize_,
            WindowsProtection(protection),
            &previousProtection)) {
        throw std::runtime_error(
            "VirtualProtect failed for native memory region");
    }
#elif defined(__linux__)
    if (mprotect(
            address_,
            mappedSize_,
            LinuxProtection(protection)) != 0) {
        throw std::runtime_error(
            "mprotect failed for native memory region");
    }
#endif

    protection_ = protection;

    if (memory::HasProtection(
            protection,
            memory::Protection::Execute)) {
        FlushCode(address_, mappedSize_);
    }
}

std::byte* NativeMemoryRegion::Data() noexcept {
    return static_cast<std::byte*>(address_);
}

const std::byte* NativeMemoryRegion::Data() const noexcept {
    return static_cast<const std::byte*>(address_);
}

std::size_t NativeMemoryRegion::Size() const noexcept {
    return size_;
}

std::size_t NativeMemoryRegion::MappedSize() const noexcept {
    return mappedSize_;
}

memory::Protection
NativeMemoryRegion::Protection() const noexcept {
    return protection_;
}

bool NativeMemoryRegion::Empty() const noexcept {
    return address_ == nullptr;
}

void NativeMemoryRegion::Release() noexcept {
    if (address_ == nullptr) {
        return;
    }

#if defined(_WIN32)
    static_cast<void>(
        VirtualFree(
            address_,
            0,
            MEM_RELEASE));
#elif defined(__linux__)
    static_cast<void>(
        munmap(
            address_,
            mappedSize_));
#endif

    address_ = nullptr;
    size_ = 0;
    mappedSize_ = 0;
    protection_ = memory::Protection::None;
}

} // namespace ps5emu::runtime
