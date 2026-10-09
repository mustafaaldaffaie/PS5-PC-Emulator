#include <ps5emu/graphics/VulkanInstanceProbe.hpp>

#include <cassert>

int main() {
    using ps5emu::graphics::VulkanInstanceProbe;
    using ps5emu::graphics::VulkanProbeStatus;
    using ps5emu::graphics::VulkanRuntimeLoader;

    {
        VulkanRuntimeLoader empty;
        const auto result =
            VulkanInstanceProbe::Probe(empty);

        assert(!result.Ready());
        assert(
            result.status ==
            VulkanProbeStatus::RuntimeUnavailable);
        assert(result.physicalDeviceCount == 0);
    }

    {
        auto loader =
            VulkanRuntimeLoader::TryLoadDefault();

        const auto result =
            VulkanInstanceProbe::Probe(loader);

        if (!loader.IsLoaded()) {
            assert(
                result.status ==
                VulkanProbeStatus::RuntimeUnavailable);
        } else {
            assert(
                result.status !=
                VulkanProbeStatus::RuntimeUnavailable);

            if (result.Ready()) {
                assert(
                    result.status ==
                    VulkanProbeStatus::Ready);
            }
        }
    }

    return 0;
}
