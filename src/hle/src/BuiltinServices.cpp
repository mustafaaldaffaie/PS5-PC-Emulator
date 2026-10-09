#include <ps5emu/hle/BuiltinServices.hpp>

#include <ps5emu/hle/BasicLibc.hpp>
#include <ps5emu/hle/KernelFile.hpp>
#include <ps5emu/hle/KernelMutex.hpp>
#include <ps5emu/hle/KernelThread.hpp>
#include <ps5emu/hle/KernelTime.hpp>

namespace ps5emu::hle {

void BuiltinServices::Register(HleRegistry& registry) {
    KernelTime::Register(registry, "libkernel");
    KernelFile::Register(registry, "libkernel");
    KernelThread::Register(registry, "libkernel");
    KernelMutex::Register(registry, "libkernel");

    BasicLibc::Register(registry, "libc");
    BasicLibc::Register(registry, "libSceLibcInternal");
}

HleRegistry BuiltinServices::CreateRegistry() {
    HleRegistry registry;
    Register(registry);
    return registry;
}

} // namespace ps5emu::hle
