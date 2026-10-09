#pragma once

#include <cstdint>

namespace ps5emu::graphics::vkabi {

#if defined(_WIN32)
#define PS5EMU_VK_CALL __stdcall
#else
#define PS5EMU_VK_CALL
#endif

using VkFlags = std::uint32_t;
using VkQueueFlags = std::uint32_t;
using VkResult = std::int32_t;
using VkStructureType = std::int32_t;

struct VkInstance_T;
struct VkPhysicalDevice_T;
struct VkDevice_T;

using VkInstance = VkInstance_T*;
using VkPhysicalDevice = VkPhysicalDevice_T*;
using VkDevice = VkDevice_T*;

constexpr VkResult kSuccess = 0;
constexpr VkResult kIncomplete = 5;

constexpr VkStructureType kStructureTypeApplicationInfo = 0;
constexpr VkStructureType kStructureTypeInstanceCreateInfo = 1;
constexpr VkStructureType kStructureTypeDeviceQueueCreateInfo = 2;
constexpr VkStructureType kStructureTypeDeviceCreateInfo = 3;

constexpr std::uint32_t kApiVersion10 = 1u << 22;
constexpr VkQueueFlags kQueueGraphicsBit = 0x00000001u;

struct VkApplicationInfo {
    VkStructureType sType;
    const void* pNext;
    const char* pApplicationName;
    std::uint32_t applicationVersion;
    const char* pEngineName;
    std::uint32_t engineVersion;
    std::uint32_t apiVersion;
};

struct VkInstanceCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    const VkApplicationInfo* pApplicationInfo;
    std::uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    std::uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
};

struct VkExtent3D {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t depth;
};

struct VkQueueFamilyProperties {
    VkQueueFlags queueFlags;
    std::uint32_t queueCount;
    std::uint32_t timestampValidBits;
    VkExtent3D minImageTransferGranularity;
};

struct VkDeviceQueueCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::uint32_t queueFamilyIndex;
    std::uint32_t queueCount;
    const float* pQueuePriorities;
};

struct VkDeviceCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::uint32_t queueCreateInfoCount;
    const VkDeviceQueueCreateInfo* pQueueCreateInfos;
    std::uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    std::uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
    const void* pEnabledFeatures;
};

using PfnVkVoidFunction =
    void (PS5EMU_VK_CALL*)();

using PfnVkGetInstanceProcAddr =
    PfnVkVoidFunction (PS5EMU_VK_CALL*)(
        VkInstance,
        const char*);

using PfnVkGetDeviceProcAddr =
    PfnVkVoidFunction (PS5EMU_VK_CALL*)(
        VkDevice,
        const char*);

using PfnVkCreateInstance =
    VkResult (PS5EMU_VK_CALL*)(
        const VkInstanceCreateInfo*,
        const void*,
        VkInstance*);

using PfnVkDestroyInstance =
    void (PS5EMU_VK_CALL*)(
        VkInstance,
        const void*);

using PfnVkEnumeratePhysicalDevices =
    VkResult (PS5EMU_VK_CALL*)(
        VkInstance,
        std::uint32_t*,
        VkPhysicalDevice*);

using PfnVkGetPhysicalDeviceQueueFamilyProperties =
    void (PS5EMU_VK_CALL*)(
        VkPhysicalDevice,
        std::uint32_t*,
        VkQueueFamilyProperties*);

using PfnVkCreateDevice =
    VkResult (PS5EMU_VK_CALL*)(
        VkPhysicalDevice,
        const VkDeviceCreateInfo*,
        const void*,
        VkDevice*);

using PfnVkDestroyDevice =
    void (PS5EMU_VK_CALL*)(
        VkDevice,
        const void*);

template <typename Function>
Function FunctionCast(void* pointer) noexcept {
    return reinterpret_cast<Function>(pointer);
}

template <typename Function>
Function FunctionCast(PfnVkVoidFunction pointer) noexcept {
    return reinterpret_cast<Function>(pointer);
}

} // namespace ps5emu::graphics::vkabi
