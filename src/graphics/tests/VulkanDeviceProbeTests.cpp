#include <ps5emu/graphics/VulkanDeviceProbe.hpp>

#include <cassert>

int main() {
    using ps5emu::graphics::VulkanDeviceProbe;
    using ps5emu::graphics::VulkanDeviceProbeStatus;
    using ps5emu::graphics::VulkanRuntimeLoader;

    {
        VulkanRuntimeLoader empty;

        const auto result =
            VulkanDeviceProbe::Probe(empty);

        assert(!result.Ready());
        assert(
            result.status ==
            VulkanDeviceProbeStatus::RuntimeUnavailable);
        assert(result.physicalDeviceCount == 0);
    }

    {
        auto loader =
            VulkanRuntimeLoader::TryLoadDefault();

        const auto result =
            VulkanDeviceProbe::Probe(loader);

        if (!loader.IsLoaded()) {
            assert(
                result.status ==
                VulkanDeviceProbeStatus::RuntimeUnavailable);
        } else {
            assert(
                result.status !=
                VulkanDeviceProbeStatus::RuntimeUnavailable);

            if (result.Ready()) {
                assert(
                    result.status ==
                    VulkanDeviceProbeStatus::Ready);
                assert(
                    result.physicalDeviceCount > 0);
            }
        }
    }

    return 0;
}
