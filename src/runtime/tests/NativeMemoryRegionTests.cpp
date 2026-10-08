#include <ps5emu/runtime/NativeMemoryRegion.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace {

template <typename Exception, typename Function>
bool Throws(Function&& function) {
    try {
        function();
        return false;
    } catch (const Exception&) {
        return true;
    }
}

} // namespace

int main() {
    using ps5emu::memory::HasProtection;
    using ps5emu::memory::Protection;
    using ps5emu::runtime::NativeMemoryRegion;

    assert(Throws<std::invalid_argument>([] {
        static_cast<void>(
            NativeMemoryRegion::Allocate(0));
    }));

    auto region =
        NativeMemoryRegion::Allocate(123);

    assert(!region.Empty());
    assert(region.Data() != nullptr);
    assert(region.Size() == 123);
    assert(region.MappedSize() >= region.Size());
    assert(HasProtection(
        region.Protection(),
        Protection::Read));
    assert(HasProtection(
        region.Protection(),
        Protection::Write));
    assert(!HasProtection(
        region.Protection(),
        Protection::Execute));

    const std::array<std::byte, 4> payload{
        std::byte{0x11},
        std::byte{0x22},
        std::byte{0x33},
        std::byte{0x44},
    };

    region.Write(7, payload);

    assert(region.Data()[7] == std::byte{0x11});
    assert(region.Data()[10] == std::byte{0x44});

    assert(Throws<std::out_of_range>([&] {
        region.Write(121, payload);
    }));

    assert(Throws<std::invalid_argument>([&] {
        region.Protect(
            Protection::Read |
            Protection::Write |
            Protection::Execute);
    }));

    assert(Throws<std::invalid_argument>([&] {
        region.Protect(Protection::Write);
    }));

    region.Protect(Protection::Read);

    assert(region.Protection() == Protection::Read);
    assert(region.Data()[7] == std::byte{0x11});
    assert(Throws<std::runtime_error>([&] {
        region.Write(0, payload);
    }));

    region.Protect(
        Protection::Read |
        Protection::Execute);

    assert(HasProtection(
        region.Protection(),
        Protection::Execute));
    assert(!HasProtection(
        region.Protection(),
        Protection::Write));

    region.Protect(
        Protection::Read |
        Protection::Write);

    region.Write(0, payload);
    assert(region.Data()[0] == std::byte{0x11});

    auto moved = std::move(region);
    assert(region.Empty());
    assert(region.Data() == nullptr);
    assert(region.Size() == 0);
    assert(!moved.Empty());
    assert(moved.Data()[0] == std::byte{0x11});

    auto replacement =
        NativeMemoryRegion::Allocate(64);
    replacement = std::move(moved);

    assert(moved.Empty());
    assert(!replacement.Empty());
    assert(replacement.Size() == 123);
    assert(replacement.Data()[0] == std::byte{0x11});

    return 0;
}
