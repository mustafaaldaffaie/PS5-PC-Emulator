#include <ps5emu/hle/KernelThread.hpp>

#include <atomic>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>

namespace ps5emu::hle {
namespace {

constexpr std::uint64_t kThreadHandleBase =
    0x00007ffc00000000ull;
constexpr std::uint64_t kThreadHandleStride =
    0x100ull;

std::atomic<std::uint64_t> g_nextThreadOrdinal{1};

std::uint64_t AllocateThreadHandle() {
    const auto ordinal =
        g_nextThreadOrdinal.fetch_add(
            1,
            std::memory_order_relaxed);

    if (ordinal >
        (std::numeric_limits<std::uint64_t>::max() -
         kThreadHandleBase) /
            kThreadHandleStride) {
        throw std::overflow_error(
            "Synthetic guest thread handle space is exhausted");
    }

    return kThreadHandleBase +
        ordinal * kThreadHandleStride;
}

std::uint64_t CurrentThreadHandle() {
    // HLE dispatch runs only after the native executor restores the host FS
    // base, so host TLS is valid here even when the guest itself uses FS.
    thread_local const std::uint64_t handle =
        AllocateThreadHandle();
    return handle;
}

void ThreadSelf(HleCallFrame& frame) {
    frame.returnValue =
        CurrentThreadHandle();
}

void ThreadEqual(HleCallFrame& frame) {
    frame.returnValue =
        frame.arguments[0] ==
                frame.arguments[1]
            ? 1u
            : 0u;
}

void ThreadYield(HleCallFrame& frame) {
    std::this_thread::yield();
    frame.returnValue = 0;
}

} // namespace

void KernelThread::Register(
    HleRegistry& registry,
    std::string module) {
    if (module.empty()) {
        throw std::invalid_argument(
            "Kernel thread module name cannot be empty");
    }

    const auto moduleName = module;

    registry.RegisterSymbol(
        moduleName,
        "scePthreadSelf",
        ThreadSelf);

    registry.RegisterSymbol(
        moduleName,
        "scePthreadEqual",
        ThreadEqual);

    registry.RegisterSymbol(
        moduleName,
        "scePthreadYield",
        ThreadYield);

    registry.RegisterSymbol(
        moduleName,
        "pthread_self",
        ThreadSelf);

    registry.RegisterSymbol(
        moduleName,
        "pthread_equal",
        ThreadEqual);

    registry.RegisterSymbol(
        std::move(module),
        "sched_yield",
        ThreadYield);
}

} // namespace ps5emu::hle
