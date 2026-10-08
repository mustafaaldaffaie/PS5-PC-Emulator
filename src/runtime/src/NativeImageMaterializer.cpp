#include <ps5emu/runtime/NativeImageMaterializer.hpp>

#include <limits>
#include <stdexcept>
#include <utility>

namespace ps5emu::runtime {
namespace {

bool RangeContains(
    const NativeImageMapping& mapping,
    std::uint64_t guestAddress,
    std::size_t size) noexcept {
    if (guestAddress < mapping.guestAddress) {
        return false;
    }

    const auto offset =
        guestAddress - mapping.guestAddress;

    if (offset > mapping.size) {
        return false;
    }

    return size <=
        mapping.size -
            static_cast<std::size_t>(offset);
}

} // namespace

bool NativeImage::Contains(
    std::uint64_t guestAddress,
    std::size_t size) const noexcept {
    for (const auto& mapping : mappings_) {
        if (RangeContains(
                mapping,
                guestAddress,
                size)) {
            return true;
        }
    }

    return false;
}

void* NativeImage::HostAddress(
    std::uint64_t guestAddress,
    std::size_t size) {
    if (!Contains(guestAddress, size)) {
        throw std::out_of_range(
            "Guest address is outside the native image");
    }

    if (guestAddress >
        std::numeric_limits<std::uintptr_t>::max()) {
        throw std::overflow_error(
            "Guest address does not fit the host pointer width");
    }

    return reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(guestAddress));
}

const void* NativeImage::HostAddress(
    std::uint64_t guestAddress,
    std::size_t size) const {
    if (!Contains(guestAddress, size)) {
        throw std::out_of_range(
            "Guest address is outside the native image");
    }

    if (guestAddress >
        std::numeric_limits<std::uintptr_t>::max()) {
        throw std::overflow_error(
            "Guest address does not fit the host pointer width");
    }

    return reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(guestAddress));
}

const std::vector<NativeImageMapping>&
NativeImage::Mappings() const noexcept {
    return mappings_;
}

NativeImage NativeImageMaterializer::Materialize(
    const memory::GuestMemory& memory) {
    NativeImage image;
    image.mappings_.reserve(
        memory.Mappings().size());

    for (const auto& mapping : memory.Mappings()) {
        if (mapping.guestAddress >
            std::numeric_limits<std::uintptr_t>::max()) {
            throw std::overflow_error(
                "Guest mapping address does not fit the host pointer width");
        }

        auto region =
            NativeMemoryRegion::AllocateAt(
                static_cast<std::uintptr_t>(
                    mapping.guestAddress),
                mapping.size);

        region.Write(0, mapping.data);
        region.Protect(mapping.protection);

        NativeImageMapping nativeMapping{
            .guestAddress = mapping.guestAddress,
            .size = mapping.size,
            .protection = mapping.protection,
            .region = std::move(region),
        };

        image.mappings_.push_back(
            std::move(nativeMapping));
    }

    return image;
}

} // namespace ps5emu::runtime
