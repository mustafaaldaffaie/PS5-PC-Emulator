#include <ps5emu/graphics/VulkanInstanceProbe.hpp>

#include "VulkanAbi.hpp"

namespace ps5emu::graphics {

VulkanProbeResult VulkanInstanceProbe::Probe(
    const VulkanRuntimeLoader& loader) noexcept {
    using namespace vkabi;

    VulkanProbeResult result;

    if (!loader.IsLoaded()) {
        result.status =
            VulkanProbeStatus::RuntimeUnavailable;
        return result;
    }

    const auto getInstanceProcAddr =
        FunctionCast<PfnVkGetInstanceProcAddr>(
            loader.Resolve("vkGetInstanceProcAddr"));

    if (getInstanceProcAddr == nullptr) {
        result.status =
            VulkanProbeStatus::EntryPointsUnavailable;
        return result;
    }

    const auto createInstance =
        FunctionCast<PfnVkCreateInstance>(
            getInstanceProcAddr(
                nullptr,
                "vkCreateInstance"));

    if (createInstance == nullptr) {
        result.status =
            VulkanProbeStatus::EntryPointsUnavailable;
        return result;
    }

    const VkApplicationInfo applicationInfo{
        .sType = kStructureTypeApplicationInfo,
        .pNext = nullptr,
        .pApplicationName = "PS5-PC-Emulator",
        .applicationVersion = 1,
        .pEngineName = "PS5-PC-Emulator",
        .engineVersion = 1,
        .apiVersion = kApiVersion10,
    };

    const VkInstanceCreateInfo createInfo{
        .sType = kStructureTypeInstanceCreateInfo,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &applicationInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = nullptr,
    };

    VkInstance instance = nullptr;
    const auto createResult =
        createInstance(
            &createInfo,
            nullptr,
            &instance);

    result.nativeResult = createResult;

    if (createResult != kSuccess ||
        instance == nullptr) {
        result.status =
            VulkanProbeStatus::InstanceCreationFailed;
        return result;
    }

    const auto destroyInstance =
        FunctionCast<PfnVkDestroyInstance>(
            getInstanceProcAddr(
                instance,
                "vkDestroyInstance"));
    const auto enumeratePhysicalDevices =
        FunctionCast<PfnVkEnumeratePhysicalDevices>(
            getInstanceProcAddr(
                instance,
                "vkEnumeratePhysicalDevices"));

    if (destroyInstance == nullptr ||
        enumeratePhysicalDevices == nullptr) {
        if (destroyInstance != nullptr) {
            destroyInstance(instance, nullptr);
        }

        result.status =
            VulkanProbeStatus::EntryPointsUnavailable;
        return result;
    }

    std::uint32_t physicalDeviceCount = 0;
    const auto enumerateResult =
        enumeratePhysicalDevices(
            instance,
            &physicalDeviceCount,
            nullptr);

    destroyInstance(instance, nullptr);

    result.nativeResult = enumerateResult;
    result.physicalDeviceCount = physicalDeviceCount;

    if (enumerateResult != kSuccess &&
        enumerateResult != kIncomplete) {
        result.status =
            VulkanProbeStatus::PhysicalDeviceEnumerationFailed;
        return result;
    }

    result.status = VulkanProbeStatus::Ready;
    return result;
}

} // namespace ps5emu::graphics
