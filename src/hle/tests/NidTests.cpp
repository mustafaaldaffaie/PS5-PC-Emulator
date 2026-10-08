#include <ps5emu/hle/Nid.hpp>

#include <array>
#include <cassert>
#include <stdexcept>
#include <string_view>

namespace {

struct Vector {
    std::string_view name;
    std::string_view nid;
};

constexpr std::array kVectors{
    Vector{"memcpy", "Q3VBxCXhUHs"},
    Vector{"memmove", "+P6FRGH4LfA"},
    Vector{"sceKernelCreateSema", "188x57JYp0g"},
    Vector{"sceKernelOpen", "1G3lF1Gg1k8"},
    Vector{"sceKernelSignalSema", "4czppHBiriw"},
    Vector{"sceKernelWrite", "4wSze92BhLI"},
    Vector{"sceKernelGetProcParam", "959qrazPIrg"},
    Vector{"scePthreadMutexLock", "9UK1vLZQft4"},
    Vector{"sceKernelRead", "Cg4srZ6TKbU"},
    Vector{"sceKernelCreateEqueue", "D0OdFMjp46I"},
    Vector{"sceKernelMmap", "PGhQHd-dzv8"},
    Vector{"sceNetSocket", "Q4qBuN-c0ZM"},
    Vector{"sceVideoOutOpen", "Up36PTk687E"},
    Vector{"scePthreadCondWait", "WKAXJ4XBPQ4"},
    Vector{"sceKernelWaitSema", "Zxa0VhQVTsk"},
    Vector{"sceAudioOutOpen", "ekNvsT22rsY"},
    Vector{"scePadOpen", "xk0AcarP3V4"},
};

template <typename Function>
bool ThrowsInvalidArgument(Function&& function) {
    try {
        function();
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

} // namespace

int main() {
    for (const auto& vector : kVectors) {
        const auto nid = ps5emu::hle::Nid::Compute(vector.name);
        assert(nid == vector.nid);
        assert(ps5emu::hle::Nid::IsValid(nid));
    }

    assert(ps5emu::hle::Nid::IsValid("ABCDEFGHIJK"));
    assert(!ps5emu::hle::Nid::IsValid("ABCDEFGHIJ"));
    assert(!ps5emu::hle::Nid::IsValid("ABCDEFGHIJ_"));

    assert(ThrowsInvalidArgument([] {
        static_cast<void>(ps5emu::hle::Nid::Compute(""));
    }));

    return 0;
}
