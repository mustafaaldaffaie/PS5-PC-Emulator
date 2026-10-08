# SCE ELF Compatibility Notes

## Scope

The parser supports standard ELF64 x86-64 dynamic metadata plus SCE extensions
used by the PlayStation 4 and PlayStation 5 executable families.

The implementation is independent and uses public format research. It does not
embed proprietary SDK headers, firmware, keys, or system libraries.

## Dynamic table storage

PlayStation 4 binaries may store SCE dynamic tables inside
PT_SCE_DYNLIBDATA (0x61000000). In that layout, SCE table references are
offsets relative to the SCE dynamic-data segment.

PlayStation 5 binaries can also use ordinary ELF DT_STRTAB and DT_SYMTAB
virtual addresses through PT_LOAD while still using newer SCE identity tags.
The parser keeps these reference models separate.

## Supported SCE table tags

- DT_SCE_JMPREL: 0x61000029
- DT_SCE_PLTREL: 0x6100002b
- DT_SCE_PLTRELSZ: 0x6100002d
- DT_SCE_RELA: 0x6100002f
- DT_SCE_RELASZ: 0x61000031
- DT_SCE_RELAENT: 0x61000033
- DT_SCE_STRTAB: 0x61000035
- DT_SCE_STRSZ: 0x61000037
- DT_SCE_SYMTAB: 0x61000039
- DT_SCE_SYMENT: 0x6100003b
- DT_SCE_SYMTABSZ: 0x6100003f

## Module and library metadata

Both legacy and next-generation forms are recognized.

Legacy tags:

- module info: 0x6100000d
- needed module: 0x6100000f
- export library: 0x61000013
- import library: 0x61000015

Next-generation tags:

- module info: 0x61000043
- needed module: 0x61000045
- export library: 0x61000047
- import library: 0x61000049

Module records pack the string-table offset into bits 31:0, minor version into
39:32, major version into 47:40, and module ID into 63:48. Library records use
bits 47:32 for the library version and 63:48 for the library ID.

Qualified SCE import symbols use the form NID#libraryId#moduleId. The NID is an
11-character value using Sony's base64 alphabet. Library and module IDs use the
same alphabet and are decoded to 16-bit IDs before metadata lookup.

## Current limitations

The project does not yet execute guest code. SELF container handling, TLS,
native HLE trampolines, complete module loading, graphics translation, and
game compatibility remain future work.

## Public references

- OpenOrbis PS4 ELF Specification:
  https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/wiki/PS4-ELF-Specification
- OpenOrbis PS4 ELF Dynlib Data notes:
  https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/wiki/PS4-ELF-Specification---Dynlib-Data
- Kyty PS4/PS5 ELF constants:
  https://github.com/KytyPS5/KytyPS5/blob/master/src/loader/elf.h
