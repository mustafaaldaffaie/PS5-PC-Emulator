# Runtime Integration

The runtime layer connects parsed guest executable metadata to host-side
compatibility services.

## Import resolution

The ELF layer discovers imported symbols and their relocations. The HLE layer
stores host-side service implementations by module and NID. The runtime layer
bridges these systems without assuming a console-specific symbol encoding.

An ImportIdentityResolver converts an ELF import into a module and NID. This
is intentionally injectable. A future module-metadata parser can provide the
real console identity mapping without changing the HLE registry or ELF parser.

Resolution produces two collections:

- bindings for imports with a known identity and registered HLE service;
- unresolved imports with an explicit failure reason.

The next stage will assign callable trampoline addresses to resolved bindings
and use those addresses while applying guest relocations.
