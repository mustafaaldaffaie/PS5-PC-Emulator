#include <ps5emu/hle/HleRegistry.hpp>

#include <cassert>
#include <stdexcept>

namespace {

template <typename Function>
bool ThrowsRuntimeError(Function&& function) {
    try {
        function();
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

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
    ps5emu::hle::HleRegistry registry;

    registry.Register(
        "libkernel",
        "TEST_NID",
        "sceKernelTest",
        [](ps5emu::hle::HleCallFrame& frame) {
            frame.returnValue =
                frame.arguments[0] + frame.arguments[1];
        });

    assert(registry.Size() == 1);

    const auto* service =
        registry.Find("libkernel", "TEST_NID");
    assert(service != nullptr);
    assert(service->module == "libkernel");
    assert(service->nid == "TEST_NID");
    assert(service->debugName == "sceKernelTest");

    ps5emu::hle::HleCallFrame frame;
    frame.arguments[0] = 20;
    frame.arguments[1] = 22;

    assert(registry.Invoke("libkernel", "TEST_NID", frame));
    assert(frame.returnValue == 42);
    assert(frame.errorCode == 0);

    assert(!registry.Invoke("libkernel", "MISSING_NID", frame));
    assert(registry.Find("missing", "TEST_NID") == nullptr);

    assert(ThrowsRuntimeError([&] {
        registry.Register(
            "libkernel",
            "TEST_NID",
            "duplicate",
            [](ps5emu::hle::HleCallFrame&) {});
    }));

    assert(ThrowsInvalidArgument([&] {
        registry.Register(
            "",
            "NID",
            "invalid",
            [](ps5emu::hle::HleCallFrame&) {});
    }));

    assert(ThrowsInvalidArgument([&] {
        registry.Register(
            "libkernel",
            "",
            "invalid",
            [](ps5emu::hle::HleCallFrame&) {});
    }));

    registry.Clear();
    assert(registry.Size() == 0);

    return 0;
}
