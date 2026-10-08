#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <span>
#include <vector>

#include <ps5emu/memory/GuestMemory.hpp>
#include <ps5emu/runtime/NativeMemoryRegion.hpp>

namespace ps5emu::runtime {

struct NativeImageMapping {
    std::uint64_t guestAddress = 0;
    std::size_t size = 0;
    memory::Protection protection = memory::Protection::None;
};

class NativeImage final {
public:
    NativeImage() = default;

    NativeImage(const NativeImage&) = delete;
    NativeImage& operator=(const NativeImage&) = delete;

    NativeImage(NativeImage&& other) noexcept;
    NativeImage& operator=(NativeImage&& other) noexcept;

    [[nodiscard]] bool
    Contains(std::uint64_t guestAddress,
             std::size_t size) const;

    [[nodiscard]] void*
    HostAddress(std::uint64_t guestAddress,
                std::size_t size = 1);

    [[nodiscard]] const void*
    HostAddress(std::uint64_t guestAddress,
                std::size_t size = 1) const;

    [[nodiscard]] std::vector<NativeImageMapping>
    Mappings() const;

    [[nodiscard]] std::optional<NativeImageMapping>
    FindMapping(std::uint64_t guestAddress,
                std::size_t size) const;

    // Adds a new fixed-address native mapping after initial materialization.
    // The guest address must satisfy the host allocation granularity. The
    // mapping is committed only after allocation, initialization, and final
    // protection all succeed.
    void AddMapping(
        std::uint64_t guestAddress,
        std::size_t size,
        memory::Protection protection,
        std::span<const std::byte> initialData = {});

    // Adds every mapping from a staging GuestMemory as one transaction.
    // No mapping metadata or native reservation is committed unless the
    // entire batch can be allocated, initialized, and protected.
    void AddMappings(
        const memory::GuestMemory& memory);

private:
    friend class NativeImageMaterializer;

    mutable std::shared_mutex mutex_;
    std::vector<NativeImageMapping> mappings_;
    std::vector<NativeMemoryRegion> reservations_;
};

class NativeImageMaterializer final {
public:
    [[nodiscard]] static NativeImage
    Materialize(const memory::GuestMemory& memory);
};

} // namespace ps5emu::runtime
