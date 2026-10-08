# Runtime Integration

The runtime layer connects parsed guest executable metadata to host-side
compatibility services.

## Import resolution

The ELF layer discovers imported symbols and their relocations. The HLE layer
stores host-side service implementations by module and NID. The runtime layer
bridges these systems without assuming a console-specific symbol encoding.

SCE-qualified symbol identities are decoded from their module and library
metadata and resolved against the HLE registry. Resolved services receive
synthetic guest thunk addresses. The linker writes those guest-visible
addresses into relocation targets.

## Synthetic HLE thunks

The thunk table reserves an executable guest-memory arena. Each HLE service is
assigned one stable slot. Current slots contain INT3 trap bytes rather than
native host-call machine code.

## Guest-call ABI dispatch

The current dispatcher implements the integer and pointer argument portion of
the x86-64 System V ABI used by the guest-facing HLE call frame:

- arguments 1 through 6 come from RDI, RSI, RDX, RCX, R8, and R9;
- arguments 7 and 8 are read from the stack after the return address;
- guest-memory access is attached to the HLE frame;
- the HLE return value is copied back to RAX;
- the HLE frame error field is returned to the host-side dispatcher caller.

## INT3 trap completion

HleTrapHandler models the control-flow side of an intercepted HLE thunk. x86
advances RIP past an INT3 before reporting a breakpoint, so the handler looks
up the thunk at RIP minus one. It validates the guest return address, dispatches
the HLE service, consumes the return address from the guest stack, and resumes
at that address.

This is deliberately OS-neutral. Windows vectored/structured exception
integration and Linux signal handling are not implemented yet.

Floating-point/vector arguments, variadic metadata, structure-return rules,
and native exception hookup remain future work.

## Preparation flow

The SCE executable preparer parses the image, resolves supported HLE imports,
allocates stable thunk addresses, links the executable, applies relocations,
and installs the thunk arena transactionally. If any stage fails, caller-owned
guest memory and thunk state are left unchanged.

Native guest instruction execution is still outside this stage.
