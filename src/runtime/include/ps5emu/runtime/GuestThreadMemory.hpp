#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include <ps5emu/elf/Elf64.hpp>
#include <ps5emu/memory/GuestMemory.hpp>

namespace ps5emu::runtime {

struct SysvGuestContext;

struct GuestThreadMemoryOptions {
    std::uint64_t stackAddress = 0;
    std::size_t stackSize = 0;
    std::optional<std::uint64_t> tlsAddress;
    std::uint64_t stackGuard = 0;
};

struct GuestThreadMemoryLayout {
    std::uint64_t stackAddress = 0;
    std::size_t stackSize = 0;
    std::uint64_t initialStackPointer = 0;
    std::optional<std::uint64_t> tlsAddress;
    std::size_t tlsSize = 0;
    std::size_t tlsBlockSize = 0;
    std::uint64_t tlsAlignment = 0;
    std::optional<std::uint64_t> threadPointer;
    std::size_t threadControlBlockSize = 0;
};

class GuestThreadMemory final {
public:
    // Stack, TLS block, and thread-control block are committed together. If
    // validation or mapping fails, the caller's guest memory remains unchanged.
    [[nodiscard]] static GuestThreadMemoryLayout
    Create(std::span<const std::byte> executableBytes,
           const elf::Image& image,
           memory::GuestMemory& memory,
           const GuestThreadMemoryOptions& options);

    // Initializes the guest stack pointer and x86-64 FS base from a previously
    // created thread layout.
    static void ApplyToContext(const GuestThreadMemoryLayout& layout,
                               SysvGuestContext& context);
};

} // namespace ps5emu::runtime
