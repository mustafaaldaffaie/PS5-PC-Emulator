#include <ps5emu/runtime/GuestCallDispatcher.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

class TestFiles final
    : public ps5emu::hle::GuestFileSystemAccess {
public:
    ps5emu::hle::GuestFileResult Open(
        std::string_view,
        std::int32_t,
        std::uint16_t) override { return {}; }

    ps5emu::hle::GuestFileResult Close(
        std::int32_t) override { return {}; }

    ps5emu::hle::GuestFileResult Read(
        std::int32_t,
        std::span<std::byte>) override { return {}; }

    ps5emu::hle::GuestFileResult Write(
        std::int32_t,
        std::span<const std::byte>) override { return {}; }

    ps5emu::hle::GuestFileResult Seek(
        std::int32_t,
        std::int64_t,
        std::int32_t) override { return {}; }
};

class TestThreads final
    : public ps5emu::hle::GuestThreadAccess {
public:
    ps5emu::hle::GuestThreadCreateResult Create(
        const ps5emu::hle::GuestThreadCreateRequest&) override {
        return {};
    }

    ps5emu::hle::GuestThreadJoinResult Join(
        std::uint64_t) override {
        return {};
    }

    std::uint64_t CurrentThreadHandle() const noexcept override {
        return 0x7777;
    }
};

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
    using ps5emu::runtime::GuestCallDispatcher;
    using ps5emu::runtime::HleThunkTable;
    using ps5emu::runtime::SysvGuestContext;

    ps5emu::hle::HleRegistry registry;
    registry.Register(
        "libkernel",
        "TESTNID0001",
        "sceKernelEightArgs",
        [](ps5emu::hle::HleCallFrame& frame) {
            assert(frame.memory != nullptr);
            assert(frame.threads != nullptr);
            assert(frame.threads->CurrentThreadHandle() == 0x7777);
            assert(frame.files != nullptr);

            std::uint64_t sum = 0;
            for (const auto value : frame.arguments) {
                sum += value;
            }

            const std::array<std::byte, 1> marker{
                std::byte{0x5a},
            };
            frame.memory->Write(0x100080, marker);

            frame.returnValue = sum;
            frame.errorCode = -42;
        });

    HleThunkTable thunks;
    const auto* service =
        registry.Find("libkernel", "TESTNID0001");
    assert(service != nullptr);

    const auto thunkAddress = thunks.Bind(*service);

    GuestMemory memory;
    memory.Map(
        0x100000,
        0x1000,
        Protection::Read | Protection::Write);

    const std::uint64_t rsp = 0x100100;

    WriteU64(memory, rsp, 0xfeedface);
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
        .rax = 999,
        .fsBase = 0x710020,
        .rbx = 0x1111,
        .rbp = 0x2222,
        .r10 = 0x3333,
        .r11 = 0x4444,
        .r12 = 0x5555,
        .r13 = 0x6666,
        .r14 = 0x7777,
        .r15 = 0x8888,
        .rflags = 0x246,
    };

    TestThreads threads;
    TestFiles files;

    const auto result =
        GuestCallDispatcher::Dispatch(
            thunkAddress,
            context,
            memory,
            registry,
            thunks,
            &threads,
            &files);

    assert(result.handled);
    assert(result.errorCode == -42);
    assert(context.rax == 36);
    assert(context.fsBase == 0x710020);
    assert(context.rbx == 0x1111);
    assert(context.rbp == 0x2222);
    assert(context.r10 == 0x3333);
    assert(context.r11 == 0x4444);
    assert(context.r12 == 0x5555);
    assert(context.r13 == 0x6666);
    assert(context.r14 == 0x7777);
    assert(context.r15 == 0x8888);
    assert(context.rflags == 0x246);
    assert(memory.Read(0x100080, 1)[0] == std::byte{0x5a});

    {
        auto unchanged = context;
        unchanged.rax = 1234;

        const auto missing =
            GuestCallDispatcher::Dispatch(
                0x1234,
                unchanged,
                memory,
                registry,
                thunks,
                &threads);

        assert(!missing.handled);
        assert(missing.errorCode == 0);
        assert(unchanged.rax == 1234);
        assert(unchanged.fsBase == 0x710020);
    }

    {
        SysvGuestContext overflowing;
        overflowing.rsp =
            std::numeric_limits<std::uint64_t>::max();

        assert(ThrowsRuntimeError([&] {
            static_cast<void>(
                GuestCallDispatcher::Dispatch(
                    thunkAddress,
                    overflowing,
                    memory,
                    registry,
                    thunks,
                    &threads,
                    &files));
        }));
    }

    return 0;
}
