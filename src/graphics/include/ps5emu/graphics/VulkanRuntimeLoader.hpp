#pragma once

#include <cstddef>
#include <string>

namespace ps5emu::graphics {

class VulkanRuntimeLoader final {
public:
    VulkanRuntimeLoader() noexcept = default;

    VulkanRuntimeLoader(
        const VulkanRuntimeLoader&) = delete;
    VulkanRuntimeLoader& operator=(
        const VulkanRuntimeLoader&) = delete;

    VulkanRuntimeLoader(
        VulkanRuntimeLoader&& other) noexcept;
    VulkanRuntimeLoader& operator=(
        VulkanRuntimeLoader&& other) noexcept;

    ~VulkanRuntimeLoader();

    [[nodiscard]] static VulkanRuntimeLoader
    TryLoadDefault();

    [[nodiscard]] bool IsLoaded() const noexcept;

    [[nodiscard]] void*
    Resolve(const char* symbolName) const noexcept;

    [[nodiscard]] const std::string&
    LibraryName() const noexcept;

private:
    VulkanRuntimeLoader(
        void* libraryHandle,
        std::string libraryName) noexcept;

    void Release() noexcept;

    void* libraryHandle_ = nullptr;
    std::string libraryName_;
};

} // namespace ps5emu::graphics
