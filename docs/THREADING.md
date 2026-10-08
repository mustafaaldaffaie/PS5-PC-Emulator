# Guest Thread Runtime

## Current baseline

The native runtime can create and join guest pthreads through the HLE layer.

Each worker thread receives:

- a dedicated guest stack mapping;
- a dedicated PT_TLS block and thread-control block when the executable uses TLS;
- an independent x86-64 FS base;
- a stable synthetic guest thread handle;
- its own NativeHleExecutor instance;
- access to the shared HLE registry, thunk table, native image, and syscall trap metadata.

Native image metadata is synchronized for concurrent reads and dynamic mapping
publication. Native breakpoint routing is keyed by host thread, so multiple
guest threads can execute HLE trap slots concurrently.

## Supported entry path

The baseline supports scePthreadCreate, scePthreadJoin, pthread_create, and
pthread_join when the pthread attribute pointer is null. A worker entry point
receives the pthread argument in RDI and its RAX value becomes the join return
value.

## Current limitations

Non-default pthread attributes are rejected until the pthread attribute HLE
layer is implemented. Worker stack and TLS address arenas are configurable but
are currently retained for the lifetime of the native execution object instead
of being unmapped immediately after join.

This is an execution baseline, not a claim of complete pthread compatibility.
