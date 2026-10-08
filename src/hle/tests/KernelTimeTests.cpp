#include <ps5emu/hle/KernelTime.hpp>
#include <ps5emu/hle/Nid.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

class TestMemory final
    : public ps5emu::hle::GuestMemoryAccess {
public:
    explicit TestMemory(std::size_t size)
        : bytes_(size, std::byte{0}) {
    }

    void Read(
        std::uint64_t guestAddress,
        std::span<std::byte> output) const override {
        if (guestAddress > bytes_.size()) {
            throw std::runtime_error(
                "Test read is out of range");
        }

        const auto offset =
            static_cast<std::size_t>(
                guestAddress);

        if (output.size() >
            bytes_.size() - offset) {
            throw std::runtime_error(
                "Test read is out of range");
        }

        std::copy_n(
            bytes_.begin() +
                static_cast<std::ptrdiff_t>(
                    offset),
            output.size(),
            output.begin());
    }

    void Write(
        std::uint64_t guestAddress,
        std::span<const std::byte> input) override {
        if (guestAddress > bytes_.size()) {
            throw std::runtime_error(
                "Test write is out of range");
        }

        const auto offset =
            static_cast<std::size_t>(
                guestAddress);

        if (input.size() >
            bytes_.size() - offset) {
            throw std::runtime_error(
                "Test write is out of range");
        }

        std::copy(
            input.begin(),
            input.end(),
            bytes_.begin() +
                static_cast<std::ptrdiff_t>(
                    offset));
    }

    template <typename T>
    void Store(
        std::uint64_t address,
        const T& value) {
        std::array<std::byte, sizeof(T)> bytes{};
        std::memcpy(
            bytes.data(),
            &value,
            sizeof(value));
        Write(address, bytes);
    }

    template <typename T>
    T Load(std::uint64_t address) const {
        std::array<std::byte, sizeof(T)> bytes{};
        Read(address, bytes);

        T value{};
        std::memcpy(
            &value,
            bytes.data(),
            sizeof(value));
        return value;
    }

private:
    std::vector<std::byte> bytes_;
};

struct GuestTimespec {
    std::int64_t seconds = 0;
    std::int64_t nanoseconds = 0;
};

struct GuestTimeval {
    std::int64_t seconds = 0;
    std::int64_t microseconds = 0;
};

const ps5emu::hle::HleService& Find(
    const ps5emu::hle::HleRegistry& registry,
    const char* name) {
    const auto nid =
        ps5emu::hle::Nid::Compute(name);
    const auto* service =
        registry.Find(
            "libkernel",
            nid);

    assert(service != nullptr);
    return *service;
}

std::uint64_t Invoke(
    const ps5emu::hle::HleRegistry& registry,
    const char* name,
    TestMemory* memory = nullptr,
    std::uint64_t argument0 = 0,
    std::uint64_t argument1 = 0) {
    ps5emu::hle::HleCallFrame frame;
    frame.arguments[0] = argument0;
    frame.arguments[1] = argument1;
    frame.memory = memory;

    Find(registry, name).handler(frame);
    return frame.returnValue;
}

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
    constexpr std::uint64_t sceFault =
        0x8002000eull;
    constexpr std::uint64_t sceInvalid =
        0x80020016ull;

    ps5emu::hle::HleRegistry registry;
    ps5emu::hle::KernelTime::Register(
        registry,
        "libkernel");

    assert(registry.Size() == 9);

    TestMemory memory(256);

    assert(
        Invoke(
            registry,
            "sceKernelGetProcessTimeCounterFrequency") ==
        1000000000ull);

    const auto counterBefore =
        Invoke(
            registry,
            "sceKernelGetProcessTimeCounter");
    const auto timeBefore =
        Invoke(
            registry,
            "sceKernelGetProcessTime");

    assert(
        Invoke(
            registry,
            "sceKernelUsleep",
            nullptr,
            0) == 0);

    assert(
        Invoke(
            registry,
            "sceKernelSleep",
            nullptr,
            0) == 0);

    const auto counterAfter =
        Invoke(
            registry,
            "sceKernelGetProcessTimeCounter");
    const auto timeAfter =
        Invoke(
            registry,
            "sceKernelGetProcessTime");

    assert(counterAfter >= counterBefore);
    assert(timeAfter >= timeBefore);

    assert(
        Invoke(
            registry,
            "sceKernelClockGettime",
            &memory,
            0,
            32) == 0);

    const auto realtime =
        memory.Load<GuestTimespec>(32);
    assert(realtime.seconds > 0);
    assert(realtime.nanoseconds >= 0);
    assert(realtime.nanoseconds < 1000000000ll);

    assert(
        Invoke(
            registry,
            "sceKernelClockGettime",
            &memory,
            4,
            64) == 0);

    const auto monotonic =
        memory.Load<GuestTimespec>(64);
    assert(monotonic.seconds >= 0);
    assert(monotonic.nanoseconds >= 0);
    assert(monotonic.nanoseconds < 1000000000ll);

    assert(
        Invoke(
            registry,
            "sceKernelClockGetres",
            &memory,
            4,
            96) == 0);

    const auto resolution =
        memory.Load<GuestTimespec>(96);
    assert(resolution.seconds >= 0);
    assert(resolution.nanoseconds >= 0);
    assert(
        resolution.seconds != 0 ||
        resolution.nanoseconds != 0);

    assert(
        Invoke(
            registry,
            "sceKernelGettimeofday",
            &memory,
            128) == 0);

    const auto timeval =
        memory.Load<GuestTimeval>(128);
    assert(timeval.seconds > 0);
    assert(timeval.microseconds >= 0);
    assert(timeval.microseconds < 1000000ll);

    const GuestTimespec request{};
    memory.Store(
        160,
        request);

    assert(
        Invoke(
            registry,
            "sceKernelNanosleep",
            &memory,
            160,
            192) == 0);

    const auto remaining =
        memory.Load<GuestTimespec>(192);
    assert(remaining.seconds == 0);
    assert(remaining.nanoseconds == 0);

    memory.Store(
        160,
        GuestTimespec{
            .seconds = 0,
            .nanoseconds = 1000000000ll,
        });

    assert(
        Invoke(
            registry,
            "sceKernelNanosleep",
            &memory,
            160) ==
        sceInvalid);

    assert(
        Invoke(
            registry,
            "sceKernelClockGettime",
            &memory,
            999,
            32) ==
        sceInvalid);

    assert(
        Invoke(
            registry,
            "sceKernelClockGettime",
            &memory,
            0,
            0) ==
        sceFault);

    assert(
        Invoke(
            registry,
            "sceKernelGettimeofday",
            &memory,
            300) ==
        sceFault);

    assert(
        Invoke(
            registry,
            "sceKernelNanosleep",
            &memory,
            0) ==
        sceFault);

    assert(ThrowsOverflow([&] {
        static_cast<void>(
            Invoke(
                registry,
                "sceKernelUsleep",
                nullptr,
                std::numeric_limits<std::uint64_t>::max()));
    }));

    assert(ThrowsOverflow([&] {
        static_cast<void>(
            Invoke(
                registry,
                "sceKernelSleep",
                nullptr,
                std::numeric_limits<std::uint64_t>::max()));
    }));

    return 0;
}
