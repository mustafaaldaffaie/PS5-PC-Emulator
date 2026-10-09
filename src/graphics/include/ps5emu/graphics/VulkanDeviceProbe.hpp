#pragma once

#include <cstdint>

#include <ps5emu/graphics/VulkanRuntimeLoader.hpp>

namespace ps5emu::graphics {

enum class VulkanDeviceProbeStatus : std::uint8_t {
    RuntimeUnavailable,
    EntryPointsUnavailable,
    InstanceCreationFailed,
    PhysicalDeviceEnumerationFailed,
    NoPhysicalDevices,
    NoGraphicsQueue,
    DeviceCreationFailed,
    Ready
};

struct VulkanDeviceProbeResult {
    VulkanDeviceProbeStatus status =
        VulkanDeviceProbeStatus::RuntimeUnavailable;
    std::int32_t nativeResult = 0;
    std::uint32_t physicalDeviceCount = 0;
    std::uint32_t selectedPhysicalDeviceIndex = 0;
    std::uint32_t graphicsQueueFamilyIndex = 0;

    [[nodiscard]] bool Ready() const noexcept {
        return status ==
            VulkanDeviceProbeStatus::Ready;
    }
};

class VulkanDeviceProbe final {
public:
    [[nodiscard]] static VulkanDeviceProbeResult
    Probe(const VulkanRuntimeLoader& loader) noexcept;
};

} // namespace ps5emu::graphics
