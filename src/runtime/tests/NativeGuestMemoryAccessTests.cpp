#include <ps5emu/runtime/HleTrapHandler.hpp>
#include <ps5emu/runtime/NativeGuestMemoryAccess.hpp>
#include <ps5emu/runtime/NativeImageMaterializer.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
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

void WriteU64(
    ps5emu::runtime::NativeGuestMemoryAccess& memory,
    std::uint64_t address,
    std::uint64_t value) {
    std::array<std::byte, sizeof(value)> bytes{};
    std::memcpy(
        bytes.data(),
        &value,
        sizeof(value));
    memory.Write(
        address,
        bytes);
}

} // namespace

int main() {
    using ps5emu::memory::GuestMemory;
    using ps5emu::memory::Protection;
    using ps5emu::runtime::HleThunkTable;
    using ps5emu::runtime::HleThunkTableOptions;
    using ps5emu::runtime::HleTrapHandler;
    using ps5emu::runtime::NativeGuestMemoryAccess;
    using ps5emu::runtime::NativeImageMaterializer;
    using ps5emu::runtime::SysvGuestContext;

    constexpr std::uint64_t base =
        0x0000200007000000ull;
    constexpr std::uint64_t dataAddress =
        base + 0x10000ull;
    constexpr std::uint64_t stackAddress =
        base + 0x20000ull;
    constexpr std::uint64_t thunkAddress =
        base + 0x30000ull;
    constexpr std::uint64_t executeOnlyAddress =
        base + 0x40000ull;

    GuestMemory source;
    source.Map(
        dataAddress,
        0x1000,
        Protection::Read |
            Protection::Write);
    source.Map(
        stackAddress,
        0x1000,
        Protection::Read |
            Protection::Write);
    source.Map(
        executeOnlyAddress,
        0x1000,
        Protection::Execute);

    const std::array<std::byte, 4> original{
        std::byte{0x10},
        std::byte{0x20},
        std::byte{0x30},
        std::byte{0x40},
    };
    source.Initialize(
        dataAddress,
        original);

    ps5emu::hle::HleRegistry registry;
    registry.Register(
        "libkernel",
        "NATIVEHLE01",
        "sceNativeHleTest",
        [dataAddress](ps5emu::hle::HleCallFrame& frame) {
            std::uint64_t sum = 0;
            for (const auto argument :
                 frame.arguments) {
                sum += argument;
            }

            const std::array<std::byte, 1> marker{
                std::byte{0x5a},
            };
            frame.memory->Write(
                dataAddress,
                marker);

            frame.returnValue = sum;
            frame.errorCode = -77;
        });

    HleThunkTable thunks(
        HleThunkTableOptions{
            .baseAddress = thunkAddress,
            .slotSize = 16,
            .capacity = 4,
        });

    const auto* service =
        registry.Find(
            "libkernel",
            "NATIVEHLE01");
    assert(service != nullptr);
    assert(
        thunks.Bind(*service) ==
        thunkAddress);

    thunks.InstallOrUpdate(
        source);

    auto nativeImage =
        NativeImageMaterializer::Materialize(
            source);

    NativeGuestMemoryAccess nativeMemory(
        nativeImage);

    std::array<std::byte, 4> readBack{};
    nativeMemory.Read(
        dataAddress,
        readBack);
    assert(readBack == original);

    const std::array<std::byte, 2> changed{
        std::byte{0xaa},
        std::byte{0xbb},
    };
    nativeMemory.Write(
        dataAddress + 1,
        changed);

    std::array<std::byte, 4> nativeChanged{};
    nativeMemory.Read(
        dataAddress,
        nativeChanged);

    assert(nativeChanged[0] == std::byte{0x10});
    assert(nativeChanged[1] == std::byte{0xaa});
    assert(nativeChanged[2] == std::byte{0xbb});
    assert(nativeChanged[3] == std::byte{0x40});

    const auto sourceStillOriginal =
        source.Read(
            dataAddress,
            original.size());
    assert(sourceStillOriginal[1] == std::byte{0x20});
    assert(sourceStillOriginal[2] == std::byte{0x30});

    assert(Throws<std::runtime_error>([&] {
        const std::array<std::byte, 1> value{
            std::byte{0x90},
        };
        nativeMemory.Write(
            thunkAddress,
            value);
    }));

    assert(Throws<std::runtime_error>([&] {
        std::array<std::byte, 1> value{};
        nativeMemory.Read(
            executeOnlyAddress,
            value);
    }));

    const std::uint64_t rsp =
        stackAddress + 0x100;
    constexpr std::uint64_t returnAddress =
        base + 0x5555;

    WriteU64(
        nativeMemory,
        rsp,
        returnAddress);
    WriteU64(
        nativeMemory,
        rsp + 8,
        7);
    WriteU64(
        nativeMemory,
        rsp + 16,
        8);

    SysvGuestContext context{
        .rdi = 1,
        .rsi = 2,
        .rdx = 3,
        .rcx = 4,
        .r8 = 5,
        .r9 = 6,
        .rsp = rsp,
        .rax = 0,
        .rip = thunkAddress + 1,
    };

    const auto result =
        HleTrapHandler::HandleInt3(
            context,
            nativeMemory,
            registry,
            thunks);

    assert(result.handled);
    assert(result.errorCode == -77);
    assert(context.rax == 36);
    assert(context.rip == returnAddress);
    assert(context.rsp == rsp + 8);

    std::array<std::byte, 1> marker{};
    nativeMemory.Read(
        dataAddress,
        marker);
    assert(marker[0] == std::byte{0x5a});

    assert(
        source.Read(
            dataAddress,
            1)[0] ==
        std::byte{0x10});

    return 0;
}
