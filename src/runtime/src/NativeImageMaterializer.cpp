#include <ps5emu/runtime/NativeImageMaterializer.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ps5emu::runtime {
namespace {

struct AddressRange {
    std::uint64_t start = 0;
    std::uint64_t end = 0;
};

std::uint64_t CheckedEnd(
    std::uint64_t start,
    std::size_t size,
    const char* message) {
    if (static_cast<std::uint64_t>(size) >
        std::numeric_limits<std::uint64_t>::max() - start) {
        throw std::overflow_error(message);
    }

    return start +
        static_cast<std::uint64_t>(size);
}

std::uint64_t AlignDown(
    std::uint64_t value,
    std::size_t alignment) {
    return value -
        value % static_cast<std::uint64_t>(alignment);
}

std::uint64_t AlignUp(
    std::uint64_t value,
    std::size_t alignment,
    const char* message) {
    const auto remainder =
        value % static_cast<std::uint64_t>(alignment);

    if (remainder == 0) {
        return value;
    }

    const auto extra =
        static_cast<std::uint64_t>(alignment) -
        remainder;

    if (value >
        std::numeric_limits<std::uint64_t>::max() - extra) {
        throw std::overflow_error(message);
    }

    return value + extra;
}

AddressRange PageRange(
    const memory::Mapping& mapping,
    std::size_t pageSize) {
    const auto end =
        CheckedEnd(
            mapping.guestAddress,
            mapping.size,
            "Guest mapping end address overflows");

    return AddressRange{
        .start =
            AlignDown(
                mapping.guestAddress,
                pageSize),
        .end =
            AlignUp(
                end,
                pageSize,
                "Guest mapping page range overflows"),
    };
}

bool RangesOverlap(
    const AddressRange& left,
    const AddressRange& right) noexcept {
    return left.start < right.end &&
        right.start < left.end;
}

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

std::size_t FindReservation(
    std::span<const AddressRange> reservations,
    std::uint64_t guestAddress,
    std::size_t size) {
    const auto end =
        CheckedEnd(
            guestAddress,
            size,
            "Guest mapping reservation lookup overflows");

    for (std::size_t index = 0;
         index < reservations.size();
         ++index) {
        if (guestAddress >= reservations[index].start &&
            end <= reservations[index].end) {
            return index;
        }
    }

    throw std::runtime_error(
        "Guest mapping has no native reservation");
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
    const auto pageSize =
        NativeMemoryRegion::SystemPageSize();
    const auto allocationGranularity =
        NativeMemoryRegion::AllocationGranularity();

    const auto& guestMappings =
        memory.Mappings();

    std::vector<AddressRange> pageRanges;
    pageRanges.reserve(guestMappings.size());

    for (const auto& mapping : guestMappings) {
        if (mapping.guestAddress >
            std::numeric_limits<std::uintptr_t>::max()) {
            throw std::overflow_error(
                "Guest mapping address does not fit the host pointer width");
        }

        pageRanges.push_back(
            PageRange(mapping, pageSize));
    }

    for (std::size_t left = 0;
         left < guestMappings.size();
         ++left) {
        for (std::size_t right = left + 1;
             right < guestMappings.size();
             ++right) {
            if (!RangesOverlap(
                    pageRanges[left],
                    pageRanges[right])) {
                continue;
            }

            if (guestMappings[left].protection !=
                guestMappings[right].protection) {
                throw std::runtime_error(
                    "Guest mappings require conflicting native page protections");
            }
        }
    }

    std::vector<AddressRange> reservations;
    reservations.reserve(guestMappings.size());

    for (const auto& mapping : guestMappings) {
        const auto end =
            CheckedEnd(
                mapping.guestAddress,
                mapping.size,
                "Guest mapping reservation end overflows");

        AddressRange range{
            .start =
                AlignDown(
                    mapping.guestAddress,
                    allocationGranularity),
            .end =
                AlignUp(
                    end,
                    allocationGranularity,
                    "Guest reservation range overflows"),
        };

        reservations.push_back(range);
    }

    std::sort(
        reservations.begin(),
        reservations.end(),
        [](const AddressRange& left,
           const AddressRange& right) {
            if (left.start != right.start) {
                return left.start < right.start;
            }
            return left.end < right.end;
        });

    std::vector<AddressRange> merged;
    merged.reserve(reservations.size());

    for (const auto& range : reservations) {
        if (merged.empty() ||
            range.start >= merged.back().end) {
            merged.push_back(range);
            continue;
        }

        merged.back().end =
            std::max(
                merged.back().end,
                range.end);
    }

    NativeImage image;
    image.mappings_.reserve(
        guestMappings.size());
    image.reservations_.reserve(
        merged.size());

    for (const auto& reservation : merged) {
        const auto size64 =
            reservation.end - reservation.start;

        if (size64 >
            std::numeric_limits<std::size_t>::max()) {
            throw std::overflow_error(
                "Native reservation is too large for this host");
        }

        image.reservations_.push_back(
            NativeMemoryRegion::AllocateAt(
                static_cast<std::uintptr_t>(
                    reservation.start),
                static_cast<std::size_t>(
                    size64)));
    }

    for (const auto& mapping : guestMappings) {
        const auto reservationIndex =
            FindReservation(
                merged,
                mapping.guestAddress,
                mapping.size);

        const auto offset64 =
            mapping.guestAddress -
            merged[reservationIndex].start;

        if (offset64 >
            std::numeric_limits<std::size_t>::max()) {
            throw std::overflow_error(
                "Guest mapping offset is too large for this host");
        }

        image.reservations_[reservationIndex].Write(
            static_cast<std::size_t>(offset64),
            mapping.data);

        image.mappings_.push_back(
            NativeImageMapping{
                .guestAddress = mapping.guestAddress,
                .size = mapping.size,
                .protection = mapping.protection,
            });
    }

    for (auto& reservation : image.reservations_) {
        reservation.Protect(
            memory::Protection::None);
    }

    for (const auto& mapping : guestMappings) {
        const auto reservationIndex =
            FindReservation(
                merged,
                mapping.guestAddress,
                mapping.size);

        const auto offset64 =
            mapping.guestAddress -
            merged[reservationIndex].start;

        image.reservations_[reservationIndex].ProtectRange(
            static_cast<std::size_t>(offset64),
            mapping.size,
            mapping.protection);
    }

    return image;
}

} // namespace ps5emu::runtime
