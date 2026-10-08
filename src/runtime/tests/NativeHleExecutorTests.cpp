#include <ps5emu/runtime/NativeHleExecutor.hpp>
#include <ps5emu/runtime/NativeSyscallInterceptor.hpp>

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

    {
        constexpr std::uint64_t supportedCode =
            base + 0x80000ull;
        constexpr std::uint64_t supportedStack =
            base + 0x90000ull;

        GuestMemory supportedMemory;
        supportedMemory.Map(
            supportedCode,
            0x1000,
            Protection::Read |
                Protection::Execute);
        supportedMemory.Map(
            supportedStack,
            0x4000,
            Protection::Read |
                Protection::Write);

        const std::array<std::byte, 12> supportedBytes{
            std::byte{0xb8},
            std::byte{0x14},
            std::byte{0x00},
            std::byte{0x00},
            std::byte{0x00},
            std::byte{0x0f},
            std::byte{0x05},
            std::byte{0x48},
            std::byte{0x83},
            std::byte{0xc0},
            std::byte{0x01},
            std::byte{0xc3},
        };

        supportedMemory.Initialize(
            supportedCode,
            supportedBytes);

        const auto supportedTraps =
            ps5emu::runtime::NativeSyscallInterceptor::Rewrite(
                supportedMemory);

        auto supportedImage =
            NativeImageMaterializer::Materialize(
                supportedMemory);

        ps5emu::hle::HleRegistry emptyRegistry;
        HleThunkTable emptyThunks(
            HleThunkTableOptions{
                .baseAddress = base + 0xa0000ull,
                .slotSize = 16,
                .capacity = 4,
            });

        SysvGuestContext supportedContext{
            .rsp = supportedStack + 0x2000,
            .rip = supportedCode,
            .rflags = 0x203,
        };

        NativeHleExecutor supportedExecutor;
        const auto supportedResult =
            supportedExecutor.Run(
                supportedContext,
                supportedImage,
                emptyRegistry,
                emptyThunks,
                supportedTraps);

        assert(supportedResult.handledTrapCount == 0);
        assert(supportedResult.handledSyscallCount == 1);
        assert(!supportedResult.interceptedSyscall);
        assert(supportedResult.syscallNumber == 20);
        assert(
            supportedResult.syscallAddress ==
            supportedCode + 5);
        assert(supportedContext.rax == 1001);
        assert(supportedContext.rip == 0);
        assert((supportedContext.rflags & 1u) == 0);
    }

    {
        constexpr std::uint64_t syscallCode =
            base + 0x50000ull;
        constexpr std::uint64_t syscallStack =
            base + 0x60000ull;

        GuestMemory syscallMemory;
        syscallMemory.Map(
            syscallCode,
            0x1000,
            Protection::Read |
                Protection::Execute);
        syscallMemory.Map(
            syscallStack,
            0x4000,
            Protection::Read |
                Protection::Write);

        const std::array<std::byte, 8> syscallBytes{
            std::byte{0xb8},
            std::byte{0x34},
            std::byte{0x12},
            std::byte{0x00},
            std::byte{0x00},
            std::byte{0x0f},
            std::byte{0x05},
            std::byte{0xc3},
        };

        syscallMemory.Initialize(
            syscallCode,
            syscallBytes);

        const auto syscallTraps =
            ps5emu::runtime::NativeSyscallInterceptor::Rewrite(
                syscallMemory);

        assert(syscallTraps.size() == 1);
        assert(
            syscallTraps[0].guestAddress ==
            syscallCode + 5);

        auto syscallImage =
            NativeImageMaterializer::Materialize(
                syscallMemory);

        ps5emu::hle::HleRegistry emptyRegistry;
        HleThunkTable emptyThunks(
            HleThunkTableOptions{
                .baseAddress = base + 0x70000ull,
                .slotSize = 16,
                .capacity = 4,
            });

        SysvGuestContext syscallContext{
            .rsp = syscallStack + 0x2000,
            .rip = syscallCode,
            .rflags = 0x202,
        };

        NativeHleExecutor syscallExecutor;
        const auto syscallResult =
            syscallExecutor.Run(
                syscallContext,
                syscallImage,
                emptyRegistry,
                emptyThunks,
                syscallTraps);

        assert(syscallResult.handledTrapCount == 0);
        assert(syscallResult.handledSyscallCount == 0);
        assert(syscallResult.interceptedSyscall);
        assert(syscallResult.syscallNumber == 0x1234);
        assert(
            syscallResult.syscallAddress ==
            syscallCode + 5);
        assert(
            syscallContext.rip ==
            syscallCode + 5);
    }

    return 0;
}
