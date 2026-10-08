#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <ps5emu/memory/GuestMemory.hpp>
#include <ps5emu/runtime/NativeMemoryRegion.hpp>

namespace ps5emu::runtime {

struct NativeImageMapping {
    std::uint64_t guestAddress = 0;
    std::size_t size = 0;
    memory::Protection protection = memory::Protection::None;
    NativeMemoryRegion region;
};

class NativeImage final {
public:
    NativeImage() = default;

    NativeImage(const NativeImage&) = delete;
    NativeImage& operator=(const NativeImage&) = delete;

    NativeImage(NativeImage&&) noexcept = default;
    NativeImage& operator=(NativeImage&&) noexcept = default;

    [[nodiscard]] bool
    Contains(std::uint64_t guestAddress,
             std::size_t size) const noexcept;

    [[nodiscard]] void*
    HostAddress(std::uint64_t guestAddress,
                std::size_t size = 1);

    [[nodiscard]] const void*
    HostAddress(std::uint64_t guestAddress,
                std::size_t size = 1) const;

    [[nodiscard]] const std::vector<NativeImageMapping>&
    Mappings() const noexcept;

private:
    friend class NativeImageMaterializer;

    std::vector<NativeImageMapping> mappings_;
};

class NativeImageMaterializer final {
public:
    [[nodiscard]] static NativeImage
    Materialize(const memory::GuestMemory& memory);
};

} // namespace ps5emu::runtime
