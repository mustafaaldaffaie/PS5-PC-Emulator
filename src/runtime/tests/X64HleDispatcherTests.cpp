#include <ps5emu/runtime/X64HleDispatcher.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

void WriteU64(ps5emu::memory::GuestMemory& memory,
              std::uint64_t address,
              std::uint64_t value) {
    std::array<std::byte, sizeof(value)> bytes{};
    std::memcpy(bytes.data(), &value, sizeof(value));
    memory.Write(address, bytes);
}

template <typename Function>
bool ThrowsRuntimeError(Function&& function) {
    try {
        function();
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

ps5emu::runtime::X64GuestContext MakeContext(
    std::uint64_t rip,
    std::uint64_t rsp) {
    ps5emu::runtime::X64GuestContext context;
    context.rax = 0xaaaa;
    context.rdi = 1;
    context.rsi = 2;
    context.rdx = 3;
    context.rcx = 4;
    context.r8 = 5;
    context.r9 = 6;
    context.rsp = rsp;
    context.rip = rip;
    return context;
}

} // namespace

int main() {
    constexpr std::uint64_t kThunkAddress = 0x90000000;
    constexpr std::uint64_t kStackAddress = 0x70000000;
    constexpr std::uint64_t kReturnAddress = 0x401234;

    ps5emu::hle::HleRegistry registry;
    registry.Register(
        "libkernel",
        "ABCDEFGHIJK",
        "sceKernelSynthetic",
        [](ps5emu::hle::HleCallFrame& frame) {
            std::uint64_t total = 0;
            for (const auto argument : frame.arguments) {
                total += argument;
            }
            frame.returnValue = total;
            frame.errorCode = -123;
        });

    ps5emu::runtime::HleThunkTable thunks(
        ps5emu::runtime::HleThunkTableOptions{
            .baseAddress = kThunkAddress,
            .slotSize = 16,
            .capacity = 4,
        });

    const auto* service =
        registry.Find("libkernel", "ABCDEFGHIJK");
    assert(service != nullptr);
    assert(thunks.Bind(*service) == kThunkAddress);

    ps5emu::memory::GuestMemory memory;
    memory.Map(
        kStackAddress,
        0x100,
        ps5emu::memory::Protection::Read |
            ps5emu::memory::Protection::Write);
    thunks.InstallOrUpdate(memory);

    const std::uint64_t rsp = kStackAddress + 0x40;
    WriteU64(memory, rsp, kReturnAddress);
    WriteU64(memory, rsp + 8, 7);
    WriteU64(memory, rsp + 16, 8);

    {
        auto context = MakeContext(kThunkAddress, rsp);

        const auto result =
            ps5emu::runtime::X64HleDispatcher::DispatchIfThunk(
                context,
                memory,
                thunks,
                registry);

        assert(result.handled);
        assert(result.errorCode == -123);
        assert(context.rax == 36);
        assert(context.rip == kReturnAddress);
        assert(context.rsp == rsp + 8);
        assert(context.rdi == 1);
        assert(context.rsi == 2);
    }

    {
        auto context = MakeContext(0x12345678, rsp);
        const auto before = context;

        const auto result =
            ps5emu::runtime::X64HleDispatcher::DispatchIfThunk(
                context,
                memory,
                thunks,
                registry);

        assert(!result.handled);
        assert(context.rax == before.rax);
        assert(context.rip == before.rip);
        assert(context.rsp == before.rsp);
    }

    {
        ps5emu::hle::HleRegistry emptyRegistry;
        auto context = MakeContext(kThunkAddress, rsp);
        const auto before = context;

        assert(ThrowsRuntimeError([&] {
            static_cast<void>(
                ps5emu::runtime::X64HleDispatcher::DispatchIfThunk(
                    context,
                    memory,
                    thunks,
                    emptyRegistry));
        }));

        assert(context.rax == before.rax);
        assert(context.rip == before.rip);
        assert(context.rsp == before.rsp);
    }

    {
        ps5emu::memory::GuestMemory shortStack;
        shortStack.Map(
            rsp,
            8,
            ps5emu::memory::Protection::Read |
                ps5emu::memory::Protection::Write);
        WriteU64(shortStack, rsp, kReturnAddress);

        auto context = MakeContext(kThunkAddress, rsp);
        const auto before = context;

        assert(ThrowsRuntimeError([&] {
            static_cast<void>(
                ps5emu::runtime::X64HleDispatcher::DispatchIfThunk(
                    context,
                    shortStack,
                    thunks,
                    registry));
        }));

        assert(context.rax == before.rax);
        assert(context.rip == before.rip);
        assert(context.rsp == before.rsp);
    }

    return 0;
}
