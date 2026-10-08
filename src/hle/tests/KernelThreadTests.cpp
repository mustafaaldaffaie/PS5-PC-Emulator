#include <ps5emu/hle/KernelThread.hpp>
#include <ps5emu/hle/Nid.hpp>

#include <cassert>
#include <cstdint>
#include <thread>

namespace {

const ps5emu::hle::HleService& Find(
    const ps5emu::hle::HleRegistry& registry,
    const char* name) {
    const auto* service =
        registry.Find(
            "libkernel",
            ps5emu::hle::Nid::Compute(name));

    assert(service != nullptr);
    return *service;
}

std::uint64_t Invoke(
    const ps5emu::hle::HleRegistry& registry,
    const char* name,
    std::uint64_t argument0 = 0,
    std::uint64_t argument1 = 0) {
    ps5emu::hle::HleCallFrame frame;
    frame.arguments[0] = argument0;
    frame.arguments[1] = argument1;

    Find(registry, name).handler(frame);
    return frame.returnValue;
}

} // namespace

int main() {
    ps5emu::hle::HleRegistry registry;
    ps5emu::hle::KernelThread::Register(
        registry,
        "libkernel");

    assert(registry.Size() == 6);

    const auto self =
        Invoke(
            registry,
            "scePthreadSelf");

    assert(self != 0);
    assert(
        Invoke(
            registry,
            "scePthreadSelf") ==
        self);
    assert(
        Invoke(
            registry,
            "pthread_self") ==
        self);

    assert(
        Invoke(
            registry,
            "scePthreadEqual",
            self,
            self) == 1);
    assert(
        Invoke(
            registry,
            "pthread_equal",
            self,
            self + 0x100) == 0);

    assert(
        Invoke(
            registry,
            "sched_yield") == 0);
    assert(
        Invoke(
            registry,
            "scePthreadYield") == 0);

    std::uint64_t otherThread = 0;
    std::thread thread(
        [&] {
            otherThread =
                Invoke(
                    registry,
                    "scePthreadSelf");
        });
    thread.join();

    assert(otherThread != 0);
    assert(otherThread != self);

    return 0;
}
