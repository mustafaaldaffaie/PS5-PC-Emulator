#include <ps5emu/graphics/VulkanDeviceProbe.hpp>

#include "VulkanAbi.hpp"

#include <vector>

namespace ps5emu::graphics {
namespace {

using namespace vkabi;

bool SuccessfulEnumeration(VkResult result) noexcept {
    return result == kSuccess ||
        result == kIncomplete;
}

} // namespace

VulkanDeviceProbeResult VulkanDeviceProbe::Probe(
    const VulkanRuntimeLoader& loader) noexcept {
    VulkanDeviceProbeResult result;

    if (!loader.IsLoaded()) {
        result.status =
            VulkanDeviceProbeStatus::RuntimeUnavailable;
        return result;
    }

    const auto getInstanceProcAddr =
        FunctionCast<PfnVkGetInstanceProcAddr>(
            loader.Resolve("vkGetInstanceProcAddr"));

    if (getInstanceProcAddr == nullptr) {
        result.status =
            VulkanDeviceProbeStatus::EntryPointsUnavailable;
        return result;
    }

    const auto createInstance =
        FunctionCast<PfnVkCreateInstance>(
            getInstanceProcAddr(
                nullptr,
                "vkCreateInstance"));

    if (createInstance == nullptr) {
        result.status =
            VulkanDeviceProbeStatus::EntryPointsUnavailable;
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

    const VkInstanceCreateInfo instanceCreateInfo{
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
    result.nativeResult =
        createInstance(
            &instanceCreateInfo,
            nullptr,
            &instance);

    if (result.nativeResult != kSuccess ||
        instance == nullptr) {
        result.status =
            VulkanDeviceProbeStatus::InstanceCreationFailed;
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
    const auto getQueueFamilyProperties =
        FunctionCast<
            PfnVkGetPhysicalDeviceQueueFamilyProperties>(
                getInstanceProcAddr(
                    instance,
                    "vkGetPhysicalDeviceQueueFamilyProperties"));
    const auto createDevice =
        FunctionCast<PfnVkCreateDevice>(
            getInstanceProcAddr(
                instance,
                "vkCreateDevice"));
    const auto getDeviceProcAddr =
        FunctionCast<PfnVkGetDeviceProcAddr>(
            getInstanceProcAddr(
                instance,
                "vkGetDeviceProcAddr"));

    if (destroyInstance == nullptr ||
        enumeratePhysicalDevices == nullptr ||
        getQueueFamilyProperties == nullptr ||
        createDevice == nullptr ||
        getDeviceProcAddr == nullptr) {
        if (destroyInstance != nullptr) {
            destroyInstance(instance, nullptr);
        }

        result.status =
            VulkanDeviceProbeStatus::EntryPointsUnavailable;
        return result;
    }

    std::uint32_t physicalDeviceCount = 0;
    auto enumerateResult =
        enumeratePhysicalDevices(
            instance,
            &physicalDeviceCount,
            nullptr);

    result.nativeResult = enumerateResult;
    result.physicalDeviceCount =
        physicalDeviceCount;

    if (!SuccessfulEnumeration(enumerateResult)) {
        destroyInstance(instance, nullptr);
        result.status =
            VulkanDeviceProbeStatus::PhysicalDeviceEnumerationFailed;
        return result;
    }

    if (physicalDeviceCount == 0) {
        destroyInstance(instance, nullptr);
        result.status =
            VulkanDeviceProbeStatus::NoPhysicalDevices;
        return result;
    }

    std::vector<VkPhysicalDevice>
        physicalDevices(physicalDeviceCount);

    enumerateResult =
        enumeratePhysicalDevices(
            instance,
            &physicalDeviceCount,
            physicalDevices.data());

    result.nativeResult = enumerateResult;
    result.physicalDeviceCount =
        physicalDeviceCount;

    if (!SuccessfulEnumeration(enumerateResult)) {
        destroyInstance(instance, nullptr);
        result.status =
            VulkanDeviceProbeStatus::PhysicalDeviceEnumerationFailed;
        return result;
    }

    bool queueFound = false;
    VkPhysicalDevice selectedDevice = nullptr;
    std::uint32_t selectedDeviceIndex = 0;
    std::uint32_t selectedQueueIndex = 0;

    for (std::uint32_t deviceIndex = 0;
         deviceIndex < physicalDeviceCount;
         ++deviceIndex) {
        std::uint32_t queueCount = 0;

        getQueueFamilyProperties(
            physicalDevices[deviceIndex],
            &queueCount,
            nullptr);

        if (queueCount == 0) {
            continue;
        }

        std::vector<VkQueueFamilyProperties>
            queues(queueCount);

        getQueueFamilyProperties(
            physicalDevices[deviceIndex],
            &queueCount,
            queues.data());

        for (std::uint32_t queueIndex = 0;
             queueIndex < queueCount;
             ++queueIndex) {
            if (queues[queueIndex].queueCount == 0 ||
                (queues[queueIndex].queueFlags &
                 kQueueGraphicsBit) == 0) {
                continue;
            }

            selectedDevice =
                physicalDevices[deviceIndex];
            selectedDeviceIndex =
                deviceIndex;
            selectedQueueIndex =
                queueIndex;
            queueFound = true;
            break;
        }

        if (queueFound) {
            break;
        }
    }

    if (!queueFound ||
        selectedDevice == nullptr) {
        destroyInstance(instance, nullptr);
        result.status =
            VulkanDeviceProbeStatus::NoGraphicsQueue;
        return result;
    }

    const float queuePriority = 1.0f;

    const VkDeviceQueueCreateInfo queueCreateInfo{
        .sType =
            kStructureTypeDeviceQueueCreateInfo,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex =
            selectedQueueIndex,
        .queueCount = 1,
        .pQueuePriorities =
            &queuePriority,
    };

    const VkDeviceCreateInfo deviceCreateInfo{
        .sType =
            kStructureTypeDeviceCreateInfo,
        .pNext = nullptr,
        .flags = 0,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos =
            &queueCreateInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = nullptr,
        .pEnabledFeatures = nullptr,
    };

    VkDevice device = nullptr;
    result.nativeResult =
        createDevice(
            selectedDevice,
            &deviceCreateInfo,
            nullptr,
            &device);

    result.selectedPhysicalDeviceIndex =
        selectedDeviceIndex;
    result.graphicsQueueFamilyIndex =
        selectedQueueIndex;

    if (result.nativeResult != kSuccess ||
        device == nullptr) {
        destroyInstance(instance, nullptr);
        result.status =
            VulkanDeviceProbeStatus::DeviceCreationFailed;
        return result;
    }

    const auto destroyDevice =
        FunctionCast<PfnVkDestroyDevice>(
            getDeviceProcAddr(
                device,
                "vkDestroyDevice"));

    if (destroyDevice == nullptr) {
        destroyInstance(instance, nullptr);
        result.status =
            VulkanDeviceProbeStatus::EntryPointsUnavailable;
        return result;
    }

    destroyDevice(device, nullptr);
    destroyInstance(instance, nullptr);

    result.status =
        VulkanDeviceProbeStatus::Ready;

    return result;
}

} // namespace ps5emu::graphics
