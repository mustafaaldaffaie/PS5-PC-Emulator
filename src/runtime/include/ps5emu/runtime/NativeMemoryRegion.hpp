#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <ps5emu/memory/GuestMemory.hpp>

namespace ps5emu::runtime {

class NativeMemoryRegion final {
public:
    [[nodiscard]] static NativeMemoryRegion
    Allocate(std::size_t size);

    [[nodiscard]] static NativeMemoryRegion
    AllocateAt(std::uintptr_t address,
               std::size_t size);

    [[nodiscard]] static std::size_t
    SystemPageSize();

    [[nodiscard]] static std::size_t
    AllocationGranularity();

    NativeMemoryRegion(const NativeMemoryRegion&) = delete;
    NativeMemoryRegion& operator=(const NativeMemoryRegion&) = delete;

    NativeMemoryRegion(NativeMemoryRegion&& other) noexcept;
    NativeMemoryRegion& operator=(NativeMemoryRegion&& other) noexcept;

    ~NativeMemoryRegion();

    void Write(std::size_t offset,
               std::span<const std::byte> bytes);

    void Protect(memory::Protection protection);

    void ProtectRange(std::size_t offset,
                      std::size_t size,
                      memory::Protection protection);

    [[nodiscard]] std::byte* Data() noexcept;
    [[nodiscard]] const std::byte* Data() const noexcept;
    [[nodiscard]] std::size_t Size() const noexcept;
    [[nodiscard]] std::size_t MappedSize() const noexcept;
    [[nodiscard]] memory::Protection Protection() const noexcept;
    [[nodiscard]] bool Empty() const noexcept;

private:
    [[nodiscard]] static NativeMemoryRegion
    AllocateImpl(std::uintptr_t requestedAddress,
                 std::size_t size,
                 bool fixed);

    NativeMemoryRegion(void* address,
                       std::size_t size,
                       std::size_t mappedSize,
                       memory::Protection protection) noexcept;

    void Release() noexcept;

    void* address_ = nullptr;
    std::size_t size_ = 0;
    std::size_t mappedSize_ = 0;
    memory::Protection protection_ = memory::Protection::None;
};

} // namespace ps5emu::runtime
