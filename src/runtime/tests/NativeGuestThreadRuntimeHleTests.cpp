#include <ps5emu/hle/KernelThread.hpp>
#include <ps5emu/hle/Nid.hpp>
#include <ps5emu/runtime/NativeGuestThreadRuntime.hpp>
#include <ps5emu/runtime/NativeHleExecutor.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include <ps5emu/memory/GuestMemory.hpp>
#include <ps5emu/runtime/HleThunkTable.hpp>
#include <ps5emu/runtime/NativeImageMaterializer.hpp>

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

void MovAbs(
    std::vector<std::byte>& code,
    std::byte opcode,
    std::uint64_t value) {
    code.push_back(std::byte{0x48});
    code.push_back(opcode);
    AppendU64(code, value);
}

std::vector<std::byte> MainCode(
    std::uint64_t createThunk,
    std::uint64_t joinThunk,
    std::uint64_t workerEntry,
    std::uint64_t handleAddress,
    std::uint64_t returnAddress) {
    std::vector<std::byte> code;

    // rdi = &thread
    MovAbs(
        code,
        std::byte{0xbf},
        handleAddress);

    // rsi = nullptr
    code.push_back(std::byte{0x31});
    code.push_back(std::byte{0xf6});

    // rdx = worker entry
    MovAbs(
        code,
        std::byte{0xba},
        workerEntry);

    // rcx = argument
    code.push_back(std::byte{0xb9});
    AppendU32(code, 37);

    // r8 = nullptr name
    code.push_back(std::byte{0x45});
    code.push_back(std::byte{0x31});
    code.push_back(std::byte{0xc0});

    // call scePthreadCreate
    MovAbs(
        code,
        std::byte{0xb8},
        createThunk);
    code.push_back(std::byte{0xff});
    code.push_back(std::byte{0xd0});

    // rax = &thread
    MovAbs(
        code,
        std::byte{0xb8},
        handleAddress);

    // rdi = *thread
    code.push_back(std::byte{0x48});
    code.push_back(std::byte{0x8b});
    code.push_back(std::byte{0x38});

    // rsi = &returnValue
    MovAbs(
        code,
        std::byte{0xbe},
        returnAddress);

    // call scePthreadJoin
    MovAbs(
        code,
        std::byte{0xb8},
        joinThunk);
    code.push_back(std::byte{0xff});
    code.push_back(std::byte{0xd0});

    // rax = &returnValue
    MovAbs(
        code,
        std::byte{0xb8},
        returnAddress);

    // rax = *returnValue
    code.push_back(std::byte{0x48});
    code.push_back(std::byte{0x8b});
    code.push_back(std::byte{0x00});

    code.push_back(std::byte{0xc3});
    return code;
}

std::uint64_t ReadU64(
    const ps5emu::runtime::NativeImage& image,
    std::uint64_t address) {
    std::uint64_t value = 0;

    std::memcpy(
        &value,
        image.HostAddress(
            address,
            sizeof(value)),
        sizeof(value));

    return value;
}

} // namespace

int main() {
    using ps5emu::memory::GuestMemory;
    using ps5emu::memory::Protection;
    using ps5emu::runtime::HleThunkTable;
    using ps5emu::runtime::HleThunkTableOptions;
    using ps5emu::runtime::NativeGuestThreadRuntime;
    using ps5emu::runtime::NativeGuestThreadRuntimeOptions;
    using ps5emu::runtime::NativeHleExecutor;
    using ps5emu::runtime::NativeImageMaterializer;
    using ps5emu::runtime::SysvGuestContext;

    constexpr std::uint64_t base =
        0x0000204000000000ull;
    constexpr std::uint64_t mainAddress =
        base + 0x10000ull;
    constexpr std::uint64_t workerAddress =
        base + 0x20000ull;
    constexpr std::uint64_t stackAddress =
        base + 0x30000ull;
    constexpr std::uint64_t dataAddress =
        base + 0x40000ull;
    constexpr std::uint64_t thunkAddress =
        base + 0x50000ull;

    ps5emu::hle::HleRegistry registry;
    ps5emu::hle::KernelThread::Register(
        registry,
        "libkernel");

    HleThunkTable thunks(
        HleThunkTableOptions{
            .baseAddress = thunkAddress,
            .slotSize = 16,
            .capacity = 16,
        });

    const auto* createService =
        registry.Find(
            "libkernel",
            ps5emu::hle::Nid::Compute(
                "scePthreadCreate"));
    const auto* joinService =
        registry.Find(
            "libkernel",
            ps5emu::hle::Nid::Compute(
                "scePthreadJoin"));

    assert(createService != nullptr);
    assert(joinService != nullptr);

    const auto createThunk =
        thunks.Bind(*createService);
    const auto joinThunk =
        thunks.Bind(*joinService);

    const auto mainCode =
        MainCode(
            createThunk,
            joinThunk,
            workerAddress,
            dataAddress,
            dataAddress + 8);

    const std::vector<std::byte> workerCode{
        std::byte{0x48},
        std::byte{0x89},
        std::byte{0xf8},
        std::byte{0x48},
        std::byte{0x83},
        std::byte{0xc0},
        std::byte{0x05},
        std::byte{0xc3},
    };

    GuestMemory memory;
    memory.Map(
        mainAddress,
        0x1000,
        Protection::Read |
            Protection::Execute);
    memory.Map(
        workerAddress,
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

    memory.Initialize(
        mainAddress,
        mainCode);
    memory.Initialize(
        workerAddress,
        workerCode);

    thunks.InstallOrUpdate(memory);

    auto nativeImage =
        NativeImageMaterializer::Materialize(
            memory);

    ps5emu::elf::Image image;
    std::vector<std::byte> executableBytes;

    NativeGuestThreadRuntime runtime(
        executableBytes,
        image,
        nativeImage,
        registry,
        thunks,
        {},
        NativeGuestThreadRuntimeOptions{
            .workerStackBase =
                base + 0x1000000ull,
            .workerStackStride =
                0x200000ull,
            .workerStackSize =
                0x100000u,
            .workerTlsBase =
                base + 0x2000000ull,
            .workerTlsStride =
                0x100000ull,
            .handleBase =
                0x00007ffa20000000ull,
            .handleStride =
                0x100ull,
        });

    SysvGuestContext context{
        .rsp =
            stackAddress + 0x2000,
        .rip =
            mainAddress,
        .rflags =
            0x202,
    };

    NativeHleExecutor executor;

    const auto result =
        executor.Run(
            context,
            nativeImage,
            registry,
            thunks,
            {},
            &runtime);

    assert(!result.interceptedSyscall);
    assert(result.handledTrapCount == 2);
    assert(context.rip == 0);
    assert(context.rax == 42);

    const auto workerHandle =
        ReadU64(
            nativeImage,
            dataAddress);

    assert(workerHandle != 0);
    assert(
        workerHandle !=
        runtime.MainThreadHandle());

    assert(
        ReadU64(
            nativeImage,
            dataAddress + 8) ==
        42);

    return 0;
}
