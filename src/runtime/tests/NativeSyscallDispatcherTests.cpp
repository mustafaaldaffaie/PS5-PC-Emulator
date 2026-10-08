#include <ps5emu/runtime/NativeSyscallDispatcher.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <span>
#include <vector>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

class TestMemory final : public ps5emu::hle::GuestMemoryAccess {
public:
    explicit TestMemory(std::size_t size)
        : bytes_(size, std::byte{0}) {
    }

    void Read(
        std::uint64_t guestAddress,
        std::span<std::byte> output) const override {
        if (guestAddress > bytes_.size()) {
            throw std::out_of_range(
                "Test read is out of range");
        }

        const auto offset =
            static_cast<std::size_t>(guestAddress);

        if (output.size() >
            bytes_.size() - offset) {
            throw std::out_of_range(
                "Test read is out of range");
        }

        std::memcpy(
            output.data(),
            bytes_.data() + offset,
            output.size());
    }

    void Write(
        std::uint64_t guestAddress,
        std::span<const std::byte> input) override {
        if (guestAddress > bytes_.size()) {
            throw std::out_of_range(
                "Test write is out of range");
        }

        const auto offset =
            static_cast<std::size_t>(guestAddress);

        if (input.size() >
            bytes_.size() - offset) {
            throw std::out_of_range(
                "Test write is out of range");
        }

        std::memcpy(
            bytes_.data() + offset,
            input.data(),
            input.size());
    }

    template <typename T>
    T Value(std::size_t offset) const {
        T value{};
        std::memcpy(
            &value,
            bytes_.data() + offset,
            sizeof(value));
        return value;
    }

private:
    std::vector<std::byte> bytes_;
};

struct GuestTimespec {
    std::int64_t seconds;
    std::int64_t nanoseconds;
};

struct GuestTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

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
            .parentProcessId = 4321,
            .userId = 2001,
            .effectiveUserId = 2002,
            .groupId = 3001,
            .effectiveGroupId = 3002,
        });

    for (const auto& test :
         {
             std::pair<std::uint64_t, std::uint64_t>{20, 1234},
             std::pair<std::uint64_t, std::uint64_t>{24, 2001},
             std::pair<std::uint64_t, std::uint64_t>{25, 2002},
             std::pair<std::uint64_t, std::uint64_t>{39, 4321},
             std::pair<std::uint64_t, std::uint64_t>{43, 3002},
             std::pair<std::uint64_t, std::uint64_t>{47, 3001},
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
        TestMemory memory(128);

        SysvGuestContext context;
        context.rax = 116;
        context.rdi = 16;
        context.rsi = 0xffffffffffffffffull;
        context.rflags = 0x203;

        const auto result =
            dispatcher.Dispatch(
                context,
                0x410000,
                &memory);

        assert(result.handled);
        assert(result.syscallNumber == 116);
        assert(context.rax == 0);
        assert((context.rflags & 1u) == 0);

        const auto value =
            memory.Value<GuestTimeval>(16);
        assert(value.seconds > 0);
        assert(value.microseconds >= 0);
        assert(value.microseconds < 1000000);
    }

    {
        TestMemory memory(128);

        for (const auto clockId :
             {0ull, 4ull}) {
            SysvGuestContext context;
            context.rax = 232;
            context.rdi = clockId;
            context.rsi = 32;
            context.rflags = 0x203;

            const auto result =
                dispatcher.Dispatch(
                    context,
                    0x420000,
                    &memory);

            assert(result.handled);
            assert(result.syscallNumber == 232);
            assert(context.rax == 0);
            assert((context.rflags & 1u) == 0);

            const auto value =
                memory.Value<GuestTimespec>(32);
            assert(value.seconds >= 0);
            assert(value.nanoseconds >= 0);
            assert(value.nanoseconds < 1000000000ll);
        }
    }

    {
        TestMemory memory(128);
        SysvGuestContext context;
        context.rax = 232;
        context.rdi = 999;
        context.rsi = 32;
        context.rflags = 0x202;

        const auto result =
            dispatcher.Dispatch(
                context,
                0x430000,
                &memory);

        assert(result.handled);
        assert(context.rax == 22);
        assert((context.rflags & 1u) != 0);
        assert(context.rip == 0x430002);
    }

    {
        SysvGuestContext context;
        context.rax = 116;
        context.rdi = 16;
        context.rflags = 0x202;

        const auto result =
            dispatcher.Dispatch(
                context,
                0x440000,
                nullptr);

        assert(result.handled);
        assert(context.rax == 14);
        assert((context.rflags & 1u) != 0);
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
