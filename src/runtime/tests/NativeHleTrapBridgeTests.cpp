#include <ps5emu/runtime/NativeHleTrapBridge.hpp>
#include <ps5emu/runtime/NativeMemoryRegion.hpp>

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
    using ps5emu::memory::Protection;
    using ps5emu::runtime::NativeHleTrapBridge;
    using ps5emu::runtime::NativeMemoryRegion;

    NativeHleTrapBridge bridge;

    auto trapCode =
        NativeMemoryRegion::Allocate(64);
    auto escapeCode =
        NativeMemoryRegion::Allocate(64);

    const std::array<std::byte, 7> trapBytes{
        std::byte{0xcc},
        std::byte{0xb8},
        std::byte{0x0d},
        std::byte{0x00},
        std::byte{0x00},
        std::byte{0x00},
        std::byte{0xc3},
    };

    const std::array<std::byte, 6> escapeBytes{
        std::byte{0xb8},
        std::byte{0x2a},
        std::byte{0x00},
        std::byte{0x00},
        std::byte{0x00},
        std::byte{0xc3},
    };

    trapCode.Write(
        0,
        trapBytes);
    escapeCode.Write(
        0,
        escapeBytes);

    trapCode.Protect(
        Protection::Read |
        Protection::Execute);
    escapeCode.Protect(
        Protection::Read |
        Protection::Execute);

    const auto trapAddress =
        reinterpret_cast<std::uintptr_t>(
            trapCode.Data());
    const auto escapeAddress =
        reinterpret_cast<std::uintptr_t>(
            escapeCode.Data());

    {
        auto scope =
            bridge.Arm(
                trapAddress,
                16,
                1,
                escapeAddress);

        using Function = int (*)();
        const auto function =
            reinterpret_cast<Function>(
                trapAddress);

        assert(function() == 42);
        assert(scope.Captured());
        assert(
            scope.CapturedRip() ==
            trapAddress + 1);

        assert(Throws<std::runtime_error>([&] {
            auto nested =
                bridge.Arm(
                    trapAddress,
                    16,
                    1,
                    escapeAddress);
            static_cast<void>(nested);
        }));
    }

    {
        auto scope =
            bridge.Arm(
                trapAddress,
                16,
                1,
                escapeAddress);

        assert(!scope.Captured());
        assert(scope.CapturedRip() == 0);
    }

    assert(Throws<std::invalid_argument>([&] {
        auto scope =
            bridge.Arm(
                trapAddress,
                0,
                1,
                escapeAddress);
        static_cast<void>(scope);
    }));

    assert(Throws<std::invalid_argument>([&] {
        auto scope =
            bridge.Arm(
                trapAddress,
                16,
                0,
                escapeAddress);
        static_cast<void>(scope);
    }));

    assert(Throws<std::invalid_argument>([&] {
        auto scope =
            bridge.Arm(
                trapAddress,
                16,
                1,
                0);
        static_cast<void>(scope);
    }));

    return 0;
}
