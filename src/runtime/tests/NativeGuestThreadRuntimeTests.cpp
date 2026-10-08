#include <ps5emu/runtime/NativeGuestThreadRuntime.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <ps5emu/memory/GuestMemory.hpp>
#include <ps5emu/runtime/NativeImageMaterializer.hpp>

int main() {
    using ps5emu::memory::GuestMemory;
    using ps5emu::memory::Protection;
    using ps5emu::runtime::HleThunkTable;
    using ps5emu::runtime::HleThunkTableOptions;
    using ps5emu::runtime::NativeGuestThreadRuntime;
    using ps5emu::runtime::NativeGuestThreadRuntimeOptions;
    using ps5emu::runtime::NativeImageMaterializer;

    constexpr std::uint64_t base =
        0x0000203000000000ull;
    constexpr std::uint64_t codeAddress =
        base + 0x10000ull;

    GuestMemory memory;
    memory.Map(
        codeAddress,
        0x1000,
        Protection::Read |
            Protection::Execute);

    // mov rax, rdi; add rax, 5; ret
    const std::array<std::byte, 8> code{
        std::byte{0x48},
        std::byte{0x89},
        std::byte{0xf8},
        std::byte{0x48},
        std::byte{0x83},
        std::byte{0xc0},
        std::byte{0x05},
        std::byte{0xc3},
    };

    memory.Initialize(
        codeAddress,
        code);

    auto nativeImage =
        NativeImageMaterializer::Materialize(
            memory);

    ps5emu::hle::HleRegistry registry;
    HleThunkTable thunks(
        HleThunkTableOptions{
            .baseAddress =
                base + 0x40000ull,
            .slotSize = 16,
            .capacity = 4,
        });

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
            .stackGuard =
                0x1122334455667788ull,
            .handleBase =
                0x00007ffa10000000ull,
            .handleStride =
                0x100ull,
        });

    assert(
        runtime.CurrentThreadHandle() ==
        runtime.MainThreadHandle());
    assert(
        runtime.MainThreadHandle() ==
        0x00007ffa10000000ull);

    const auto created =
        runtime.Create(
            ps5emu::hle::GuestThreadCreateRequest{
                .entryPoint =
                    codeAddress,
                .argument = 37,
            });

    assert(created.errorCode == 0);
    assert(created.handle != 0);
    assert(
        created.handle !=
        runtime.MainThreadHandle());

    const auto joined =
        runtime.Join(
            created.handle);

    assert(joined.errorCode == 0);
    assert(joined.returnValue == 42);

    const auto joinedAgain =
        runtime.Join(
            created.handle);

    assert(
        joinedAgain.errorCode ==
        0x80020003ull);

    const auto invalidEntry =
        runtime.Create(
            ps5emu::hle::GuestThreadCreateRequest{});

    assert(
        invalidEntry.errorCode ==
        0x80020016ull);

    const auto unsupportedAttributes =
        runtime.Create(
            ps5emu::hle::GuestThreadCreateRequest{
                .attributeAddress = 0x1234,
                .entryPoint = codeAddress,
            });

    assert(
        unsupportedAttributes.errorCode ==
        0x80020016ull);

    const auto selfJoin =
        runtime.Join(
            runtime.MainThreadHandle());

    assert(
        selfJoin.errorCode ==
        0x8002000bull);

    return 0;
}
