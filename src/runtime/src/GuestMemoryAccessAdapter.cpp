#include <ps5emu/runtime/GuestMemoryAccessAdapter.hpp>

#include <algorithm>

namespace ps5emu::runtime {

GuestMemoryAccessAdapter::GuestMemoryAccessAdapter(
    memory::GuestMemory& memory) noexcept
    : memory_(memory) {
}

void GuestMemoryAccessAdapter::Read(
    std::uint64_t guestAddress,
    std::span<std::byte> output) const {
    if (output.empty()) {
        return;
    }

    const auto source =
        memory_.Read(
            guestAddress,
            output.size());

    std::copy(
        source.begin(),
        source.end(),
        output.begin());
}

void GuestMemoryAccessAdapter::Write(
    std::uint64_t guestAddress,
    std::span<const std::byte> input) {
    memory_.Write(
        guestAddress,
        input);
}

} // namespace ps5emu::runtime
