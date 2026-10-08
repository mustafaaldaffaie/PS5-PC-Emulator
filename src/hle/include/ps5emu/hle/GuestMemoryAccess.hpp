#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace ps5emu::hle {

class GuestMemoryAccess {
public:
    virtual ~GuestMemoryAccess() = default;

    virtual void Read(std::uint64_t guestAddress,
                      std::span<std::byte> output) const = 0;

    virtual void Write(std::uint64_t guestAddress,
                       std::span<const std::byte> input) = 0;
};

} // namespace ps5emu::hle
