# SCE ELF Compatibility Notes

## Scope

The parser supports the public SCE ELF dynamic-table layout used by the
PlayStation 4 toolchain family and observed in related x86-64 console
executables. PS5 compatibility must still be validated against legally
obtained test samples before behavior is considered stable.

The implementation is intentionally independent and is based on published
format information rather than proprietary SDK headers or firmware.

## Supported program header

- PT_SCE_DYNLIBDATA: 0x61000000

## Supported SCE dynamic tags

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

SCE table references are offsets relative to PT_SCE_DYNLIBDATA. Standard ELF
dynamic references continue to be interpreted as virtual addresses.

## Public references

- OpenOrbis PS4 ELF Specification:
  https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/wiki/PS4-ELF-Specification
- OpenOrbis PS4 ELF Dynlib Data notes:
  https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/wiki/PS4-ELF-Specification---Dynlib-Data
- OpenOrbis create-fself constants:
  https://github.com/OpenOrbis/create-fself/blob/master/pkg/oelf/PS4Constants.go
