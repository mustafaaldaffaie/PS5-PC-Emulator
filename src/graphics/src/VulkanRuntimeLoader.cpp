#include <ps5emu/graphics/VulkanRuntimeLoader.hpp>

#include <array>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#elif defined(__linux__)
#include <dlfcn.h>
#else
#error "VulkanRuntimeLoader currently supports Windows and Linux only"
#endif

namespace ps5emu::graphics {
namespace {

void* OpenLibrary(const char* name) noexcept {
#if defined(_WIN32)
    return reinterpret_cast<void*>(
        LoadLibraryA(name));
#elif defined(__linux__)
    return dlopen(
        name,
        RTLD_NOW | RTLD_LOCAL);
#endif
}

void CloseLibrary(void* handle) noexcept {
    if (handle == nullptr) {
        return;
    }

#if defined(_WIN32)
    static_cast<void>(
        FreeLibrary(
            reinterpret_cast<HMODULE>(handle)));
#elif defined(__linux__)
    static_cast<void>(dlclose(handle));
#endif
}

void* ResolveSymbol(
    void* handle,
    const char* name) noexcept {
    if (handle == nullptr ||
        name == nullptr ||
        *name == '\0') {
        return nullptr;
    }

#if defined(_WIN32)
    return reinterpret_cast<void*>(
        GetProcAddress(
            reinterpret_cast<HMODULE>(handle),
            name));
#elif defined(__linux__)
    return dlsym(handle, name);
#endif
}

} // namespace

VulkanRuntimeLoader::VulkanRuntimeLoader(
    void* libraryHandle,
    std::string libraryName) noexcept
    : libraryHandle_(libraryHandle),
      libraryName_(std::move(libraryName)) {
}

VulkanRuntimeLoader::VulkanRuntimeLoader(
    VulkanRuntimeLoader&& other) noexcept
    : libraryHandle_(
          std::exchange(
              other.libraryHandle_,
              nullptr)),
      libraryName_(
          std::move(other.libraryName_)) {
}

VulkanRuntimeLoader&
VulkanRuntimeLoader::operator=(
    VulkanRuntimeLoader&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    Release();

    libraryHandle_ =
        std::exchange(
            other.libraryHandle_,
            nullptr);
    libraryName_ =
        std::move(other.libraryName_);

    return *this;
}

VulkanRuntimeLoader::~VulkanRuntimeLoader() {
    Release();
}

VulkanRuntimeLoader
VulkanRuntimeLoader::TryLoadDefault() {
#if defined(_WIN32)
    constexpr std::array<const char*, 1> names{
        "vulkan-1.dll",
    };
#elif defined(__linux__)
    constexpr std::array<const char*, 2> names{
        "libvulkan.so.1",
        "libvulkan.so",
    };
#endif

    for (const auto* name : names) {
        auto* handle = OpenLibrary(name);
        if (handle == nullptr) {
            continue;
        }

        if (ResolveSymbol(
                handle,
                "vkGetInstanceProcAddr") == nullptr) {
            CloseLibrary(handle);
            continue;
        }

        return VulkanRuntimeLoader(
            handle,
            name);
    }

    return {};
}

bool VulkanRuntimeLoader::IsLoaded() const noexcept {
    return libraryHandle_ != nullptr;
}

void* VulkanRuntimeLoader::Resolve(
    const char* symbolName) const noexcept {
    return ResolveSymbol(
        libraryHandle_,
        symbolName);
}

const std::string&
VulkanRuntimeLoader::LibraryName() const noexcept {
    return libraryName_;
}

void VulkanRuntimeLoader::Release() noexcept {
    CloseLibrary(libraryHandle_);
    libraryHandle_ = nullptr;
    libraryName_.clear();
}

} // namespace ps5emu::graphics
