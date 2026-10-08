# High-Level Emulation Layer

The HLE layer maps guest platform services to host-side implementations.

## Registry

Services are identified by a module name and NID. The registry intentionally
does not expose native C++ function signatures to guest code. Each service
receives an ABI-neutral call frame containing integer argument slots, a return
value, an error field, and an optional guest-memory access interface.

## Guest memory access

HLE handlers can read and write guest addresses through GuestMemoryAccess.
The interface is independent of the core memory implementation, so the HLE
library does not need to own or map process memory itself. The runtime
dispatcher provides an adapter for the active GuestMemory instance while an
HLE call is executing.

## Basic libc bridge

BasicLibc registers the first guest-memory functions by symbol name:

- memcpy
- memmove
- memset

The module name is supplied by the caller so SCE module metadata remains the
source of module identity. The bridge currently focuses on correctness and
validation rather than optimized bulk transfers.

## Planned flow

1. Parse guest imports from ELF metadata.
2. Associate each import with its source module.
3. Resolve the module and NID through the HLE registry.
4. Allocate a synthetic guest thunk.
5. Populate an HLE call frame from guest register and stack state.
6. Attach guest-memory access for pointer-based services.
7. Invoke the registered service implementation.
8. Copy return state back to the guest context.

## Policy

HLE implementations are clean-room replacements for observable platform
behavior. The project does not embed proprietary system libraries.
