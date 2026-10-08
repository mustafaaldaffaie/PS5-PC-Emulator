# High-Level Emulation Layer

The HLE layer maps guest platform services to host-side implementations.

## Registry

Services are identified by a module name and NID. The registry intentionally
does not expose native C++ function signatures to guest code. Instead, each
service receives an ABI-neutral call frame containing integer argument slots,
a return value, and an error field.

This keeps symbol resolution separate from future x86-64 trampoline and ABI
translation work.

## Planned flow

1. Parse guest imports from ELF metadata.
2. Associate each import with its source module.
3. Resolve the module and NID through the HLE registry.
4. Generate or select a host-call trampoline.
5. Populate an HLE call frame from guest register and stack state.
6. Invoke the registered service implementation.
7. Copy return state back to the guest context.

## Policy

HLE implementations are clean-room replacements for observable platform
behavior. The project does not embed proprietary system libraries.
