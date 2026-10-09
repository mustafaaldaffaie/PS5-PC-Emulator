#include <ps5emu/graphics/VulkanInstanceProbe.hpp>

#include <cstddef>
#include <cstdint>

namespace ps5emu::graphics {
namespace {

#if defined(_WIN32)
#define PS5EMU_VK_CALL __stdcall
#else
#define PS5EMU_VK_CALL
#endif

using VkFlags = std::uint32_t;
using VkResult = std::int32_t;
using VkStructureType = std::int32_t;

struct VkInstance_T;
struct VkPhysicalDevice_T;

using VkInstance = VkInstance_T*;
using VkPhysicalDevice = VkPhysicalDevice_T*;

constexpr VkResult kVkSuccess = 0;
constexpr VkResult kVkIncomplete = 5;
constexpr VkStructureType kVkStructureTypeApplicationInfo = 0;
constexpr VkStructureType kVkStructureTypeInstanceCreateInfo = 1;
constexpr std::uint32_t kVkApiVersion10 = 1u << 22;

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

using PfnVkVoidFunction = void (PS5EMU_VK_CALL*)();

using PfnVkGetInstanceProcAddr =
    PfnVkVoidFunction (PS5EMU_VK_CALL*)(
        VkInstance,
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

template <typename Function>
Function FunctionCast(void* pointer) noexcept {
    return reinterpret_cast<Function>(pointer);
}

template <typename Function>
Function FunctionCast(PfnVkVoidFunction pointer) noexcept {
    return reinterpret_cast<Function>(pointer);
}

} // namespace

VulkanProbeResult VulkanInstanceProbe::Probe(
    const VulkanRuntimeLoader& loader) noexcept {
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
        .sType = kVkStructureTypeApplicationInfo,
        .pNext = nullptr,
        .pApplicationName = "PS5-PC-Emulator",
        .applicationVersion = 1,
        .pEngineName = "PS5-PC-Emulator",
        .engineVersion = 1,
        .apiVersion = kVkApiVersion10,
    };

    const VkInstanceCreateInfo createInfo{
        .sType = kVkStructureTypeInstanceCreateInfo,
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

    if (createResult != kVkSuccess ||
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

    if (enumerateResult != kVkSuccess &&
        enumerateResult != kVkIncomplete) {
        result.status =
            VulkanProbeStatus::PhysicalDeviceEnumerationFailed;
        return result;
    }

    result.status = VulkanProbeStatus::Ready;
    return result;
}

} // namespace ps5emu::graphics
