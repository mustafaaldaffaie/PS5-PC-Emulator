#pragma once

#include <cstdint>

#include <ps5emu/graphics/VulkanRuntimeLoader.hpp>

namespace ps5emu::graphics {

enum class VulkanProbeStatus : std::uint8_t {
    RuntimeUnavailable,
    EntryPointsUnavailable,
    InstanceCreationFailed,
    PhysicalDeviceEnumerationFailed,
    Ready
};

struct VulkanProbeResult {
    VulkanProbeStatus status =
        VulkanProbeStatus::RuntimeUnavailable;
    std::int32_t nativeResult = 0;
    std::uint32_t physicalDeviceCount = 0;

    [[nodiscard]] bool Ready() const noexcept {
        return status == VulkanProbeStatus::Ready;
    }
};

class VulkanInstanceProbe final {
public:
    [[nodiscard]] static VulkanProbeResult
    Probe(const VulkanRuntimeLoader& loader) noexcept;
};

} // namespace ps5emu::graphics
