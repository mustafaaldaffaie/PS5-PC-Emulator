#include <ps5emu/graphics/VulkanRuntimeLoader.hpp>

#include <cassert>
#include <utility>

int main() {
    using ps5emu::graphics::VulkanRuntimeLoader;

    VulkanRuntimeLoader empty;
    assert(!empty.IsLoaded());
    assert(empty.Resolve("vkGetInstanceProcAddr") == nullptr);
    assert(empty.Resolve(nullptr) == nullptr);
    assert(empty.Resolve("") == nullptr);
    assert(empty.LibraryName().empty());

    auto loader =
        VulkanRuntimeLoader::TryLoadDefault();

    if (loader.IsLoaded()) {
        assert(!loader.LibraryName().empty());
        assert(
            loader.Resolve(
                "vkGetInstanceProcAddr") != nullptr);
        assert(
            loader.Resolve(
                "vkDefinitelyNotARealVulkanSymbol") == nullptr);
    } else {
        assert(loader.LibraryName().empty());
    }

    auto moved = std::move(loader);

    assert(!loader.IsLoaded());
    assert(loader.LibraryName().empty());

    if (moved.IsLoaded()) {
        assert(
            moved.Resolve(
                "vkGetInstanceProcAddr") != nullptr);
    }

    VulkanRuntimeLoader assigned;
    assigned = std::move(moved);

    assert(!moved.IsLoaded());
    assert(moved.LibraryName().empty());

    if (assigned.IsLoaded()) {
        assert(
            assigned.Resolve(
                "vkGetInstanceProcAddr") != nullptr);
    }

    return 0;
}
