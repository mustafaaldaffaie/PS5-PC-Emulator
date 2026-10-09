# Graphics Architecture

## Direction

The graphics subsystem will translate guest GPU work into Vulkan.

The first step is a Vulkan runtime loader that does not require the Vulkan SDK
at build time. It dynamically opens the platform Vulkan loader and resolves
vkGetInstanceProcAddr.

Supported host loader names:

- Windows: vulkan-1.dll
- Linux: libvulkan.so.1, then libvulkan.so

This keeps the core build portable on CI machines that do not have Vulkan
headers or development packages installed.

## Planned layers

1. Vulkan runtime discovery and entry-point loading.
2. Vulkan instance creation and physical-device enumeration.
3. Logical device and queue selection.
4. Memory allocator and resource lifetime tracking.
5. Guest command-stream abstraction.
6. PM4 packet decoding.
7. Shader decoding and intermediate representation.
8. SPIR-V generation and pipeline caching.
9. Texture, render-target, and synchronization translation.

## Constraints

The graphics layer must remain independent from proprietary console libraries.
Guest GPU behavior is translated from observable executable and command-stream
data into public Vulkan APIs.


## Vulkan device baseline

The device probe creates a Vulkan 1.0 instance, enumerates physical devices,
finds the first queue family with graphics capability, and attempts to create
a logical device with one graphics queue.

The probe deliberately enables no optional extensions or device features yet.
This keeps device bring-up deterministic and provides a stable base for later
swapchain, memory, synchronization, and queue-submission work.

All Vulkan ABI declarations used by this layer are public Vulkan ABI types and
are kept in a private internal header so the project still builds without the
Vulkan SDK.
