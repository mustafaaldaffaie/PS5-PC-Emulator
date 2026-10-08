#include <ps5emu/hle/BuiltinServices.hpp>
#include <ps5emu/hle/Nid.hpp>

#include <cassert>

namespace {

const ps5emu::hle::HleService*
FindSymbol(const ps5emu::hle::HleRegistry& registry,
           const char* module,
           const char* name) {
    return registry.Find(
        module,
        ps5emu::hle::Nid::Compute(name));
}

} // namespace

int main() {
    const auto registry =
        ps5emu::hle::BuiltinServices::CreateRegistry();

    assert(registry.Size() >= 50);

    assert(
        FindSymbol(
            registry,
            "libkernel",
            "sceKernelGetProcessTime") != nullptr);
    assert(
        FindSymbol(
            registry,
            "libkernel",
            "sceKernelGetProcessTimeCounter") != nullptr);
    assert(
        FindSymbol(
            registry,
            "libkernel",
            "sceKernelUsleep") != nullptr);
    assert(
        FindSymbol(
            registry,
            "libkernel",
            "sceKernelClockGettime") != nullptr);
    assert(
        FindSymbol(
            registry,
            "libkernel",
            "sceKernelGettimeofday") != nullptr);
    assert(
        FindSymbol(
            registry,
            "libkernel",
            "sceKernelNanosleep") != nullptr);

    assert(
        FindSymbol(
            registry,
            "libkernel",
            "scePthreadSelf") != nullptr);
    assert(
        FindSymbol(
            registry,
            "libkernel",
            "scePthreadEqual") != nullptr);
    assert(
        FindSymbol(
            registry,
            "libkernel",
            "scePthreadYield") != nullptr);
    assert(
        FindSymbol(
            registry,
            "libkernel",
            "pthread_self") != nullptr);
    assert(
        FindSymbol(
            registry,
            "libkernel",
            "pthread_equal") != nullptr);
    assert(
        FindSymbol(
            registry,
            "libkernel",
            "sched_yield") != nullptr);

    assert(
        FindSymbol(
            registry,
            "libkernel",
            "scePthreadMutexInit") != nullptr);
    assert(
        FindSymbol(
            registry,
            "libkernel",
            "scePthreadMutexLock") != nullptr);
    assert(
        FindSymbol(
            registry,
            "libkernel",
            "scePthreadMutexTrylock") != nullptr);
    assert(
        FindSymbol(
            registry,
            "libkernel",
            "scePthreadMutexUnlock") != nullptr);
    assert(
        FindSymbol(
            registry,
            "libkernel",
            "scePthreadMutexDestroy") != nullptr);

    assert(
        FindSymbol(
            registry,
            "libc",
            "memcpy") != nullptr);
    assert(
        FindSymbol(
            registry,
            "libc",
            "strlen") != nullptr);
    assert(
        FindSymbol(
            registry,
            "libc",
            "strcmp") != nullptr);
    assert(
        FindSymbol(
            registry,
            "libc",
            "strcpy") != nullptr);
    assert(
        FindSymbol(
            registry,
            "libSceLibcInternal",
            "memchr") != nullptr);

    assert(
        FindSymbol(
            registry,
            "libSceLibcInternal",
            "memcpy") != nullptr);
    assert(
        FindSymbol(
            registry,
            "libSceLibcInternal",
            "strnlen") != nullptr);

    assert(
        FindSymbol(
            registry,
            "unknown",
            "memcpy") == nullptr);

    return 0;
}
