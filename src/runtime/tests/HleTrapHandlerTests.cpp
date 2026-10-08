#include <ps5emu/runtime/HleTrapHandler.hpp>

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
    memory.Initialize(address, bytes);
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

} // namespace

int main() {
    using ps5emu::memory::GuestMemory;
    using ps5emu::memory::Protection;
    using ps5emu::runtime::HleThunkTable;
    using ps5emu::runtime::HleTrapHandler;
    using ps5emu::runtime::SysvGuestContext;

    ps5emu::hle::HleRegistry registry;
    registry.Register(
        "libkernel",
        "TRAPTEST001",
        "sceTrapTest",
        [](ps5emu::hle::HleCallFrame& frame) {
            std::uint64_t sum = 0;
            for (const auto argument : frame.arguments) {
                sum += argument;
            }

            frame.returnValue = sum;
            frame.errorCode = -7;
        });

    HleThunkTable thunks;
    const auto* service =
        registry.Find("libkernel", "TRAPTEST001");
    assert(service != nullptr);

    const auto thunkAddress = thunks.Bind(*service);

    GuestMemory memory;
    memory.Map(
        0x200000,
        0x1000,
        Protection::Read | Protection::Write);
    thunks.InstallOrUpdate(memory);

    const std::uint64_t rsp = 0x200100;
    const std::uint64_t returnAddress = 0x401234;

    WriteU64(memory, rsp, returnAddress);
    WriteU64(memory, rsp + 8, 7);
    WriteU64(memory, rsp + 16, 8);

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
            memory,
            registry,
            thunks);

    assert(result.handled);
    assert(result.errorCode == -7);
    assert(context.rax == 36);
    assert(context.rip == returnAddress);
    assert(context.rsp == rsp + 8);

    {
        auto unknown = context;
        unknown.rip = 0x12345678;
        unknown.rsp = rsp;
        unknown.rax = 55;

        const auto unknownResult =
            HleTrapHandler::HandleInt3(
                unknown,
                memory,
                registry,
                thunks);

        assert(!unknownResult.handled);
        assert(unknown.rip == 0x12345678);
        assert(unknown.rsp == rsp);
        assert(unknown.rax == 55);
    }

    {
        SysvGuestContext zeroRip;
        zeroRip.rip = 0;

        const auto zeroResult =
            HleTrapHandler::HandleInt3(
                zeroRip,
                memory,
                registry,
                thunks);

        assert(!zeroResult.handled);
    }

    {
        GuestMemory missingStackMemory;
        HleThunkTable missingStackThunks;
        const auto missingStackAddress =
            missingStackThunks.Bind(*service);
        missingStackThunks.InstallOrUpdate(missingStackMemory);

        SysvGuestContext missingStack;
        missingStack.rip = missingStackAddress + 1;
        missingStack.rsp = 0x900000;

        assert(ThrowsRuntimeError([&] {
            static_cast<void>(
                HleTrapHandler::HandleInt3(
                    missingStack,
                    missingStackMemory,
                    registry,
                    missingStackThunks));
        }));
    }

    return 0;
}
