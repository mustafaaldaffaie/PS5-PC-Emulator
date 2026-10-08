#include <ps5emu/runtime/NativeGuestMemoryAccess.hpp>

#include <cstring>
#include <stdexcept>

namespace ps5emu::runtime {
namespace {

bool Contains(
    const NativeImageMapping& mapping,
    std::uint64_t guestAddress,
    std::size_t size) noexcept {
    if (guestAddress < mapping.guestAddress) {
        return false;
    }

    const auto offset =
        guestAddress - mapping.guestAddress;

    return offset <= mapping.size &&
        size <=
            mapping.size -
                static_cast<std::size_t>(offset);
}

} // namespace

NativeGuestMemoryAccess::NativeGuestMemoryAccess(
    NativeImage& image) noexcept
    : image_(image) {
}

void NativeGuestMemoryAccess::Read(
    std::uint64_t guestAddress,
    std::span<std::byte> output) const {
    if (output.empty()) {
        return;
    }

    const auto& mapping =
        FindMapping(
            guestAddress,
            output.size());

    if (!memory::HasProtection(
            mapping.protection,
            memory::Protection::Read)) {
        throw std::runtime_error(
            "Native guest memory region is not readable");
    }

    const auto* source =
        static_cast<const std::byte*>(
            image_.HostAddress(
                guestAddress,
                output.size()));

    std::memcpy(
        output.data(),
        source,
        output.size());
}

void NativeGuestMemoryAccess::Write(
    std::uint64_t guestAddress,
    std::span<const std::byte> input) {
    if (input.empty()) {
        return;
    }

    const auto& mapping =
        FindMapping(
            guestAddress,
            input.size());

    if (!memory::HasProtection(
            mapping.protection,
            memory::Protection::Write)) {
        throw std::runtime_error(
            "Native guest memory region is not writable");
    }

    auto* destination =
        static_cast<std::byte*>(
            image_.HostAddress(
                guestAddress,
                input.size()));

    std::memcpy(
        destination,
        input.data(),
        input.size());
}

const NativeImageMapping&
NativeGuestMemoryAccess::FindMapping(
    std::uint64_t guestAddress,
    std::size_t size) const {
    for (const auto& mapping :
         image_.Mappings()) {
        if (Contains(
                mapping,
                guestAddress,
                size)) {
            return mapping;
        }
    }

    throw std::runtime_error(
        "Native guest memory access is outside mapped regions");
}

} // namespace ps5emu::runtime
