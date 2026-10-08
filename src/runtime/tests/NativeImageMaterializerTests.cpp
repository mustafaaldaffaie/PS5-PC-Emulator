#include <ps5emu/runtime/NativeImageMaterializer.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

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
    using ps5emu::memory::GuestMemory;
    using ps5emu::memory::HasProtection;
    using ps5emu::memory::Protection;
    using ps5emu::runtime::NativeImageMaterializer;

    constexpr std::uint64_t reservationBase =
        0x0000200000000000ull;
    constexpr std::uint64_t codeAddress =
        reservationBase + 0x1000ull;
    constexpr std::uint64_t dataAddress =
        reservationBase + 0x3000ull;

    GuestMemory memory;

    memory.Map(
        codeAddress,
        0x1000,
        Protection::Read |
            Protection::Execute);

    memory.Map(
        dataAddress,
        0x1000,
        Protection::Read |
            Protection::Write);

    const std::array<std::byte, 6> code{
        std::byte{0xb8},
        std::byte{0x2a},
        std::byte{0x00},
        std::byte{0x00},
        std::byte{0x00},
        std::byte{0xc3},
    };

    memory.Initialize(codeAddress, code);

    const std::array<std::byte, 4> data{
        std::byte{0x11},
        std::byte{0x22},
        std::byte{0x33},
        std::byte{0x44},
    };

    memory.Initialize(dataAddress, data);

    auto native =
        NativeImageMaterializer::Materialize(
            memory);

    assert(native.Mappings().size() == 2);

    assert(
        reinterpret_cast<std::uintptr_t>(
            native.HostAddress(codeAddress, 4)) ==
        codeAddress);

    assert(
        reinterpret_cast<std::uintptr_t>(
            native.HostAddress(dataAddress, 4)) ==
        dataAddress);

    const auto* codeBytes =
        static_cast<const std::byte*>(
            native.HostAddress(codeAddress, 4));

    assert(codeBytes[0] == std::byte{0xb8});
    assert(codeBytes[1] == std::byte{0x2a});
    assert(codeBytes[5] == std::byte{0xc3});

#if defined(_M_X64) || defined(__x86_64__)
    using LeafFunction = std::uint64_t (*)();
    const auto function =
        reinterpret_cast<LeafFunction>(
            native.HostAddress(
                codeAddress,
                code.size()));

    assert(function() == 42);
#endif

    auto* dataBytes =
        static_cast<std::byte*>(
            native.HostAddress(dataAddress, 4));

    assert(dataBytes[0] == std::byte{0x11});
    dataBytes[0] = std::byte{0xaa};
    assert(dataBytes[0] == std::byte{0xaa});

    assert(
        HasProtection(
            native.Mappings()[0].protection,
            Protection::Execute));

    assert(
        HasProtection(
            native.Mappings()[1].protection,
            Protection::Write));

    assert(native.Contains(codeAddress, 4));
    assert(!native.Contains(
        codeAddress + 0x1000,
        1));

    assert(Throws<std::out_of_range>([&] {
        static_cast<void>(
            native.HostAddress(
                codeAddress + 0x1000,
                1));
    }));

    {
        GuestMemory unaligned;
        constexpr std::uint64_t address =
            reservationBase + 0x50123ull;

        unaligned.Map(
            address,
            16,
            Protection::Read);

        const std::array<std::byte, 2> bytes{
            std::byte{0x5a},
            std::byte{0xa5},
        };

        unaligned.Initialize(
            address,
            bytes);

        auto nativeUnaligned =
            NativeImageMaterializer::Materialize(
                unaligned);

        const auto* nativeBytes =
            static_cast<const std::byte*>(
                nativeUnaligned.HostAddress(
                    address,
                    bytes.size()));

        assert(nativeBytes[0] == std::byte{0x5a});
        assert(nativeBytes[1] == std::byte{0xa5});
    }

    {
        GuestMemory invalid;
        invalid.Map(
            reservationBase + 0x70000ull,
            0x1000,
            Protection::Read |
                Protection::Write |
                Protection::Execute);

        assert(Throws<std::invalid_argument>([&] {
            static_cast<void>(
                NativeImageMaterializer::Materialize(
                    invalid));
        }));
    }

    {
        GuestMemory conflicting;
        const auto page =
            reservationBase + 0x90000ull;

        conflicting.Map(
            page + 0x100,
            0x100,
            Protection::Read |
                Protection::Execute);

        conflicting.Map(
            page + 0x400,
            0x100,
            Protection::Read |
                Protection::Write);

        assert(Throws<std::runtime_error>([&] {
            static_cast<void>(
                NativeImageMaterializer::Materialize(
                    conflicting));
        }));
    }

    return 0;
}
