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

    constexpr std::uint64_t codeAddress =
        0x0000200000000000ull;
    constexpr std::uint64_t dataAddress =
        codeAddress + 0x20000ull;

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

    const std::array<std::byte, 4> code{
        std::byte{0x90},
        std::byte{0x90},
        std::byte{0x90},
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

    assert(codeBytes[0] == std::byte{0x90});
    assert(codeBytes[3] == std::byte{0xc3});

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
        GuestMemory invalid;
        invalid.Map(
            codeAddress + 0x40000ull,
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
        GuestMemory unaligned;
        unaligned.Map(
            codeAddress + 1,
            0x1000,
            Protection::Read);

        assert(Throws<std::invalid_argument>([&] {
            static_cast<void>(
                NativeImageMaterializer::Materialize(
                    unaligned));
        }));
    }

    return 0;
}
