#pragma once

#include <ps5emu/hle/GuestMemoryAccess.hpp>
#include <ps5emu/memory/GuestMemory.hpp>

namespace ps5emu::runtime {

class GuestMemoryAccessAdapter final
    : public hle::GuestMemoryAccess {
public:
    explicit GuestMemoryAccessAdapter(
        memory::GuestMemory& memory) noexcept;

    void Read(
        std::uint64_t guestAddress,
        std::span<std::byte> output) const override;

    void Write(
        std::uint64_t guestAddress,
        std::span<const std::byte> input) override;

private:
    memory::GuestMemory& memory_;
};

} // namespace ps5emu::runtime
