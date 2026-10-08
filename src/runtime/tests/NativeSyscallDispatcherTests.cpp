#include <ps5emu/runtime/NativeSyscallDispatcher.hpp>

#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

template <typename Function>
bool ThrowsOverflow(Function&& function) {
    try {
        function();
        return false;
    } catch (const std::overflow_error&) {
        return true;
    }
}

} // namespace

int main() {
    using ps5emu::runtime::GuestProcessIdentity;
    using ps5emu::runtime::NativeSyscallDispatcher;
    using ps5emu::runtime::SysvGuestContext;

    NativeSyscallDispatcher dispatcher(
        GuestProcessIdentity{
            .processId = 1234,
            .userId = 2001,
            .effectiveUserId = 2002,
        });

    for (const auto& test :
         {
             std::pair<std::uint64_t, std::uint64_t>{20, 1234},
             std::pair<std::uint64_t, std::uint64_t>{24, 2001},
             std::pair<std::uint64_t, std::uint64_t>{25, 2002},
         }) {
        SysvGuestContext context;
        context.rax = test.first;
        context.rip = 0x400100;
        context.rcx = 0xdead;
        context.r11 = 0xbeef;
        context.rflags = 0x203;

        const auto result =
            dispatcher.Dispatch(
                context,
                0x400100);

        assert(result.handled);
        assert(result.syscallNumber == test.first);
        assert(context.rax == test.second);
        assert(context.rip == 0x400102);
        assert(context.rcx == 0x400102);
        assert((context.rflags & 1u) == 0);
        assert(context.r11 == context.rflags);
    }

    {
        SysvGuestContext context;
        context.rax = 9999;
        context.rip = 0x500000;
        context.rcx = 1;
        context.r11 = 2;
        context.rflags = 0x203;

        const auto before = context;
        const auto result =
            dispatcher.Dispatch(
                context,
                0x500000);

        assert(!result.handled);
        assert(result.syscallNumber == 9999);
        assert(context.rax == before.rax);
        assert(context.rip == before.rip);
        assert(context.rcx == before.rcx);
        assert(context.r11 == before.r11);
        assert(context.rflags == before.rflags);
    }

    {
        SysvGuestContext context;
        context.rax = 20;

        assert(ThrowsOverflow([&] {
            static_cast<void>(
                dispatcher.Dispatch(
                    context,
                    std::numeric_limits<std::uint64_t>::max()));
        }));
    }

    return 0;
}
