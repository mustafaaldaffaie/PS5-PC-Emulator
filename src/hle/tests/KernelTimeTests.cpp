#include <ps5emu/hle/KernelTime.hpp>
#include <ps5emu/hle/Nid.hpp>

#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

const ps5emu::hle::HleService& Find(
    const ps5emu::hle::HleRegistry& registry,
    const char* name) {
    const auto nid = ps5emu::hle::Nid::Compute(name);
    const auto* service =
        registry.Find("libkernel", nid);

    assert(service != nullptr);
    return *service;
}

std::uint64_t Invoke(
    const ps5emu::hle::HleRegistry& registry,
    const char* name,
    std::uint64_t argument = 0) {
    ps5emu::hle::HleCallFrame frame;
    frame.arguments[0] = argument;
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
    ps5emu::hle::HleRegistry registry;
    ps5emu::hle::KernelTime::Register(
        registry,
        "libkernel");

    assert(registry.Size() == 5);

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
            0) == 0);

    assert(
        Invoke(
            registry,
            "sceKernelSleep",
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

    assert(ThrowsOverflow([&] {
        static_cast<void>(
            Invoke(
                registry,
                "sceKernelUsleep",
                std::numeric_limits<std::uint64_t>::max()));
    }));

    assert(ThrowsOverflow([&] {
        static_cast<void>(
            Invoke(
                registry,
                "sceKernelSleep",
                std::numeric_limits<std::uint64_t>::max()));
    }));

    return 0;
}
