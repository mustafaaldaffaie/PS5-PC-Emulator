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


## PM4 command stream

The command-stream layer currently performs Type-3 packet framing and strict
bounds validation. It extracts opcode, flags, packet word range, and command
buffer offset without assigning execution semantics to individual opcodes.

Type-3 framing uses the public packet layout:

- bits 31:30: packet type, required to be 3;
- bits 29:16: encoded word count, with two header/base words added;
- bits 15:8: opcode;
- bits 7:0: packet flags.

Opcode execution is intentionally a separate layer so packet validation can be
tested independently from graphics state mutation and Vulkan translation.
