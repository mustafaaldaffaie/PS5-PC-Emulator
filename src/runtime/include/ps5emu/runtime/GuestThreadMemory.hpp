#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include <ps5emu/elf/Elf64.hpp>
#include <ps5emu/memory/GuestMemory.hpp>

namespace ps5emu::runtime {

struct GuestThreadMemoryOptions {
    std::uint64_t stackAddress = 0;
    std::size_t stackSize = 0;
    std::optional<std::uint64_t> tlsAddress;
};

struct GuestThreadMemoryLayout {
    std::uint64_t stackAddress = 0;
    std::size_t stackSize = 0;
    std::uint64_t initialStackPointer = 0;
    std::optional<std::uint64_t> tlsAddress;
    std::size_t tlsSize = 0;
    std::uint64_t tlsAlignment = 0;
};

class GuestThreadMemory final {
public:
    // Stack and TLS mappings are committed together. If validation or mapping
    // fails, the caller's guest memory remains unchanged.
    [[nodiscard]] static GuestThreadMemoryLayout
    Create(std::span<const std::byte> executableBytes,
           const elf::Image& image,
           memory::GuestMemory& memory,
           const GuestThreadMemoryOptions& options);
};

} // namespace ps5emu::runtime
