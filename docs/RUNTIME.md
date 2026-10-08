# Runtime Integration

The runtime layer connects parsed guest executable metadata to host-side
compatibility services.

## Import resolution

The ELF layer discovers imported symbols and their relocations. The HLE layer
stores host-side service implementations by module and NID. The runtime layer
bridges these systems without coupling the HLE registry to raw ELF structures.

ImportResolver remains generic and accepts an injectable ImportIdentityResolver.
For SCE binaries, SceImportResolver decodes qualified NID#libraryId#moduleId
symbols, resolves the module ID through SCE needed-module metadata, validates
known import-library IDs when present, and produces the module + NID identity
used by the HLE registry.

Resolution produces two collections:

- bindings for imports with a known identity and registered HLE service;
- unresolved imports with an explicit failure reason.

Native guest-call trampolines are still required before HLE handlers can be
used as relocation targets.

## Single-image linking

ExecutableLinker::Load parses dynamic tables, maps PT_LOAD segments at the
requested load bias, and applies supported x86-64 RELA relocations. The entry
point and segment addresses use the same bias. Local symbol addresses use the
bias; SHN_ABS values remain absolute. Missing weak imports resolve to zero.
Required imports need an ExternalSymbolResolver returning an already valid
guest-visible address. Addresses are cached by symbol index during one load.

TLS, indirect functions, COMMON storage, and other reserved section indexes
are rejected rather than treated as ordinary symbol addresses. Relocation
targets must stay within one load segment belonging to the new image; they
cannot modify an existing unrelated mapping. A failed load leaves the caller's
memory unchanged, including when a resolver throws or a relocation fails late.

This foundation stages copies of guest memory to provide rollback. It is
intended for small research inputs; loading large titles will need a mapping
transaction that avoids copying existing allocations. Successful loading
invalidates previous spans and mapping references. Resolver callbacks must
not mutate the caller's memory or recursively load into it.

ps5emu prepare <elf-file> [load-bias] exercises the path without executing
guest code. The bias accepts decimal or 0x hexadecimal. The CLI has no native
HLE trampolines yet, so required external imports fail explicitly. inspect
continues to display metadata without linking. Neither command runs games.

Symbol rules follow the generic ELF ABI:
https://gabi.xinuos.com/elf/05-symtab.html
