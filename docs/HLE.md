# High-Level Emulation Layer

The HLE layer maps guest platform services to host-side implementations.

## Registry

Services are identified by a module name and NID. The registry intentionally
does not expose native C++ function signatures to guest code. Instead, each
service receives an ABI-neutral call frame containing integer argument slots,
a return value, and an error field.

This keeps symbol resolution separate from future x86-64 trampoline and ABI
translation work.

## NID computation

The project can compute the 11-character Sony NID for a known symbol name.
The implementation hashes the symbol name followed by the public 16-byte NID
salt with SHA-1, reverses the first eight digest bytes, and encodes them with
Sony's base64 alphabet.

Golden vectors cover libc, kernel, networking, video, audio, pad, and pthread
symbols. Forward hashing is useful for registering known HLE functions. It
does not reverse unknown NIDs; a separate name database is still planned.

## Planned flow

1. Parse guest imports from ELF metadata.
2. Decode SCE module and library identities.
3. Resolve the module and NID through the HLE registry.
4. Generate or select a host-call trampoline.
5. Populate an HLE call frame from guest register and stack state.
6. Invoke the registered service implementation.
7. Copy return state back to the guest context.

## Policy

HLE implementations are clean-room replacements for observable platform
behavior. The project does not embed proprietary system libraries.
