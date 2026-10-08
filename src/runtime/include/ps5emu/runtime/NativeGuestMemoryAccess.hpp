#pragma once

#include <ps5emu/hle/GuestMemoryAccess.hpp>
#include <ps5emu/runtime/NativeImageMaterializer.hpp>

namespace ps5emu::runtime {

class NativeGuestMemoryAccess final
    : public hle::GuestMemoryAccess {
public:
    explicit NativeGuestMemoryAccess(
        NativeImage& image) noexcept;

    void Read(
        std::uint64_t guestAddress,
        std::span<std::byte> output) const override;

    void Write(
        std::uint64_t guestAddress,
        std::span<const std::byte> input) override;

private:
    [[nodiscard]] NativeImageMapping
    FindMapping(
        std::uint64_t guestAddress,
        std::size_t size) const;

    NativeImage& image_;
};

} // namespace ps5emu::runtime
