#include <ps5emu/runtime/NativeLeafExecutor.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

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

std::vector<std::byte> GuestCode(
    std::uint64_t rbxValue,
    std::uint64_t r12Value) {
    std::vector<std::byte> code;

    const auto byte =
        [&code](std::uint8_t value) {
            code.push_back(
                static_cast<std::byte>(value));
        };

    const auto u64 =
        [&byte](std::uint64_t value) {
            for (unsigned shift = 0;
                 shift < 64;
                 shift += 8) {
                byte(static_cast<std::uint8_t>(
                    value >> shift));
            }
        };

    byte(0x48);
    byte(0x89);
    byte(0xe0);

    byte(0x48);
    byte(0xbb);
    u64(rbxValue);

    byte(0x49);
    byte(0xbc);
    u64(r12Value);

    byte(0x48);
    byte(0x83);
    byte(0xc7);
    byte(0x05);

    byte(0xc3);

    return code;
}

} // namespace

int main() {
    using ps5emu::memory::GuestMemory;
    using ps5emu::memory::Protection;
    using ps5emu::runtime::NativeImageMaterializer;
    using ps5emu::runtime::NativeLeafExecutor;
    using ps5emu::runtime::SysvGuestContext;

    constexpr std::uint64_t reservationBase =
        0x0000200005000000ull;
    constexpr std::uint64_t codeAddress =
        reservationBase + 0x1000ull;
    constexpr std::uint64_t stackAddress =
        reservationBase + 0x20000ull;
    constexpr std::uint64_t stackSize =
        0x4000ull;

    constexpr std::uint64_t expectedRbx =
        0x1122334455667788ull;
    constexpr std::uint64_t expectedR12 =
        0xaabbccddeeff0011ull;

    GuestMemory memory;
    memory.Map(
        codeAddress,
        0x1000,
        Protection::Read |
            Protection::Execute);
    memory.Map(
        stackAddress,
        static_cast<std::size_t>(stackSize),
        Protection::Read |
            Protection::Write);

    const auto code =
        GuestCode(
            expectedRbx,
            expectedR12);
    memory.Initialize(
        codeAddress,
        code);

    auto nativeImage =
        NativeImageMaterializer::Materialize(
            memory);

    NativeLeafExecutor executor;

    SysvGuestContext context{
        .rdi = 10,
        .rsi = 20,
        .rdx = 30,
        .rcx = 40,
        .r8 = 50,
        .r9 = 60,
        .rsp = stackAddress + stackSize,
        .rax = 70,
        .rip = codeAddress,
        .fsBase = 0,
        .rbx = 80,
        .rbp = 90,
        .r10 = 100,
        .r11 = 110,
        .r12 = 120,
        .r13 = 130,
        .r14 = 140,
        .r15 = 150,
        .rflags = 0x202,
    };

    const auto initialRsp =
        context.rsp;

    executor.Run(
        context,
        nativeImage);

    assert(
        context.rax ==
        initialRsp - 8);
    assert(context.rdi == 15);
    assert(context.rsi == 20);
    assert(context.rdx == 30);
    assert(context.rcx == 40);
    assert(context.r8 == 50);
    assert(context.r9 == 60);
    assert(context.rbx == expectedRbx);
    assert(context.rbp == 90);
    assert(context.r10 == 100);
    assert(context.r11 == 110);
    assert(context.r12 == expectedR12);
    assert(context.r13 == 130);
    assert(context.r14 == 140);
    assert(context.r15 == 150);
    assert(context.rsp == initialRsp);
    assert(context.rip == 0);
    assert(context.fsBase == 0);
    assert((context.rflags & 0x2u) != 0);

    {
        auto tlsContext = context;
        tlsContext.rip = codeAddress;
        tlsContext.rsp = initialRsp;
        tlsContext.fsBase = 0x1234;

        assert(Throws<std::runtime_error>([&] {
            executor.Run(
                tlsContext,
                nativeImage);
        }));
    }

    {
        GuestMemory readOnlyStackMemory;
        readOnlyStackMemory.Map(
            codeAddress + 0x10000ull,
            0x1000,
            Protection::Read |
                Protection::Execute);
        readOnlyStackMemory.Map(
            stackAddress + 0x10000ull,
            0x1000,
            Protection::Read);

        readOnlyStackMemory.Initialize(
            codeAddress + 0x10000ull,
            code);

        auto readOnlyImage =
            NativeImageMaterializer::Materialize(
                readOnlyStackMemory);

        SysvGuestContext readOnlyContext;
        readOnlyContext.rip =
            codeAddress + 0x10000ull;
        readOnlyContext.rsp =
            stackAddress + 0x11000ull;

        assert(Throws<std::runtime_error>([&] {
            executor.Run(
                readOnlyContext,
                readOnlyImage);
        }));
    }

    {
        GuestMemory nonExecutableMemory;
        nonExecutableMemory.Map(
            codeAddress + 0x30000ull,
            0x1000,
            Protection::Read |
                Protection::Write);
        nonExecutableMemory.Map(
            stackAddress + 0x30000ull,
            0x1000,
            Protection::Read |
                Protection::Write);

        nonExecutableMemory.Initialize(
            codeAddress + 0x30000ull,
            code);

        auto nonExecutableImage =
            NativeImageMaterializer::Materialize(
                nonExecutableMemory);

        SysvGuestContext nonExecutableContext;
        nonExecutableContext.rip =
            codeAddress + 0x30000ull;
        nonExecutableContext.rsp =
            stackAddress + 0x31000ull;

        assert(Throws<std::runtime_error>([&] {
            executor.Run(
                nonExecutableContext,
                nonExecutableImage);
        }));
    }

    return 0;
}
