#include <ps5emu/runtime/NativeHleExecutor.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

void AppendU32(
    std::vector<std::byte>& code,
    std::uint32_t value) {
    for (unsigned shift = 0;
         shift < 32;
         shift += 8) {
        code.push_back(
            static_cast<std::byte>(
                value >> shift));
    }
}

void AppendU64(
    std::vector<std::byte>& code,
    std::uint64_t value) {
    for (unsigned shift = 0;
         shift < 64;
         shift += 8) {
        code.push_back(
            static_cast<std::byte>(
                value >> shift));
    }
}

std::vector<std::byte> GuestCode(
    std::uint64_t thunkAddress) {
    std::vector<std::byte> code;

    code.push_back(std::byte{0xbf});
    AppendU32(code, 20);

    code.push_back(std::byte{0xbe});
    AppendU32(code, 22);

    code.push_back(std::byte{0x48});
    code.push_back(std::byte{0xb8});
    AppendU64(code, thunkAddress);

    code.push_back(std::byte{0xff});
    code.push_back(std::byte{0xd0});

    code.push_back(std::byte{0x48});
    code.push_back(std::byte{0x83});
    code.push_back(std::byte{0xc0});
    code.push_back(std::byte{0x01});

    code.push_back(std::byte{0x48});
    code.push_back(std::byte{0x89});
    code.push_back(std::byte{0xc7});

    code.push_back(std::byte{0xbe});
    AppendU32(code, 1);

    code.push_back(std::byte{0x48});
    code.push_back(std::byte{0xb8});
    AppendU64(code, thunkAddress);

    code.push_back(std::byte{0xff});
    code.push_back(std::byte{0xd0});

    code.push_back(std::byte{0x48});
    code.push_back(std::byte{0x83});
    code.push_back(std::byte{0xc0});
    code.push_back(std::byte{0x02});

    code.push_back(std::byte{0xc3});

    return code;
}

} // namespace

int main() {
    using ps5emu::memory::GuestMemory;
    using ps5emu::memory::Protection;
    using ps5emu::runtime::HleThunkTable;
    using ps5emu::runtime::HleThunkTableOptions;
    using ps5emu::runtime::NativeHleExecutor;
    using ps5emu::runtime::NativeImageMaterializer;
    using ps5emu::runtime::SysvGuestContext;

    constexpr std::uint64_t base =
        0x0000200009000000ull;
    constexpr std::uint64_t codeAddress =
        base + 0x10000ull;
    constexpr std::uint64_t stackAddress =
        base + 0x20000ull;
    constexpr std::uint64_t dataAddress =
        base + 0x30000ull;
    constexpr std::uint64_t thunkAddress =
        base + 0x40000ull;

    ps5emu::hle::HleRegistry registry;
    registry.Register(
        "libkernel",
        "NATIVELOOP1",
        "sceNativeLoopTest",
        [dataAddress](
            ps5emu::hle::HleCallFrame& frame) {
            const std::array<std::byte, 1> marker{
                std::byte{0x5a},
            };

            frame.memory->Write(
                dataAddress,
                marker);

            frame.returnValue =
                frame.arguments[0] +
                frame.arguments[1];
            frame.errorCode = -91;
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
            "NATIVELOOP1");
    assert(service != nullptr);
    assert(
        thunks.Bind(*service) ==
        thunkAddress);

    GuestMemory memory;
    memory.Map(
        codeAddress,
        0x1000,
        Protection::Read |
            Protection::Execute);
    memory.Map(
        stackAddress,
        0x4000,
        Protection::Read |
            Protection::Write);
    memory.Map(
        dataAddress,
        0x1000,
        Protection::Read |
            Protection::Write);

    const auto code =
        GuestCode(thunkAddress);
    memory.Initialize(
        codeAddress,
        code);

    thunks.InstallOrUpdate(memory);

    auto nativeImage =
        NativeImageMaterializer::Materialize(
            memory);

    SysvGuestContext context{
        .rsp = stackAddress + 0x2000,
        .rip = codeAddress,
        .rflags = 0x202,
    };
    const auto initialRsp = context.rsp;

    NativeHleExecutor executor;
    const auto result =
        executor.Run(
            context,
            nativeImage,
            registry,
            thunks);

    assert(result.handledTrapCount == 2);
    assert(result.lastErrorCode == -91);
    assert(context.rax == 46);
    assert(context.rsp == initialRsp);
    assert(context.rip == 0);

    const auto* marker =
        static_cast<const std::byte*>(
            nativeImage.HostAddress(
                dataAddress,
                1));
    assert(marker[0] == std::byte{0x5a});

    assert(
        memory.Read(
            dataAddress,
            1)[0] ==
        std::byte{0});

    return 0;
}
