#include <ps5emu/memory/GuestMemory.hpp>

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace ps5emu::memory {
namespace {

bool RangeOverflows(std::uint64_t start, std::size_t size) {
    if (size == 0) {
        return false;
    }

    return start > std::numeric_limits<std::uint64_t>::max() -
                       static_cast<std::uint64_t>(size - 1);
}

bool Contains(const Mapping& mapping,
              std::uint64_t guestAddress,
              std::size_t size) {
    if (RangeOverflows(guestAddress, size)) {
        return false;
    }

    const auto mappingEnd =
        mapping.guestAddress + static_cast<std::uint64_t>(mapping.size);
    const auto requestEnd =
        guestAddress + static_cast<std::uint64_t>(size);

    return guestAddress >= mapping.guestAddress &&
           requestEnd <= mappingEnd;
}

bool Overlaps(const Mapping& mapping,
              std::uint64_t guestAddress,
              std::size_t size) {
    const auto mappingEnd =
        mapping.guestAddress + static_cast<std::uint64_t>(mapping.size);
    const auto requestEnd =
        guestAddress + static_cast<std::uint64_t>(size);

    return guestAddress < mappingEnd && mapping.guestAddress < requestEnd;
}

} // namespace

void GuestMemory::Map(std::uint64_t guestAddress,
                      std::size_t size,
                      Protection protection) {
    if (size == 0) {
        throw std::runtime_error("Cannot map a zero-sized guest region");
    }

    if (RangeOverflows(guestAddress, size)) {
        throw std::runtime_error("Guest mapping address range overflows");
    }

    const bool hasOverlap = std::any_of(
        mappings_.begin(),
        mappings_.end(),
        [&](const Mapping& mapping) {
            return Overlaps(mapping, guestAddress, size);
        });

    if (hasOverlap) {
        throw std::runtime_error("Guest mapping overlaps an existing region");
    }

    Mapping mapping;
    mapping.guestAddress = guestAddress;
    mapping.size = size;
    mapping.protection = protection;
    mapping.data.resize(size);

    mappings_.push_back(std::move(mapping));
}

void GuestMemory::Write(std::uint64_t guestAddress,
                        std::span<const std::byte> bytes) {
    auto& mapping = FindMapping(guestAddress, bytes.size());

    const auto offset =
        static_cast<std::size_t>(guestAddress - mapping.guestAddress);

    std::memcpy(mapping.data.data() + offset,
                bytes.data(),
                bytes.size());
}

std::span<const std::byte>
GuestMemory::Read(std::uint64_t guestAddress, std::size_t size) const {
    const auto& mapping = FindMapping(guestAddress, size);

    const auto offset =
        static_cast<std::size_t>(guestAddress - mapping.guestAddress);

    return std::span<const std::byte>(
        mapping.data.data() + offset,
        size);
}

const std::vector<Mapping>& GuestMemory::Mappings() const noexcept {
    return mappings_;
}

Mapping& GuestMemory::FindMapping(std::uint64_t guestAddress,
                                  std::size_t size) {
    for (auto& mapping : mappings_) {
        if (Contains(mapping, guestAddress, size)) {
            return mapping;
        }
    }

    throw std::runtime_error("Guest memory access is outside mapped regions");
}

const Mapping& GuestMemory::FindMapping(std::uint64_t guestAddress,
                                        std::size_t size) const {
    for (const auto& mapping : mappings_) {
        if (Contains(mapping, guestAddress, size)) {
            return mapping;
        }
    }

    throw std::runtime_error("Guest memory access is outside mapped regions");
}

} // namespace ps5emu::memory
