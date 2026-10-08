# PS5-PC-Emulator

Experimental PS5 compatibility project for Windows and Linux.

## Goals

- Clean-room compatibility work focused on interoperability, research, and preservation.
- Native x86-64 execution where practical.
- High-level implementations of required platform services.
- Vulkan-based graphics backend.
- Desktop frontend with English and Arabic localization, including RTL support.
- Source code, comments, logs, identifiers, and build scripts remain English-only.

## Project status

Early development. The core parses ELF64 and SCE dynamic tables, maps guest
segments, resolves import identities against registered HLE services, and
applies a supported subset of x86-64 relocations. Single-image linking can
use injected external symbol addresses. Native guest execution, graphics,
and game compatibility are not implemented.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Requires CMake 3.24 or later and a C++23 compiler. The Qt frontend is optional
and enabled with `-DPS5EMU_BUILD_FRONTEND=ON`. Test assertions remain active in
Release builds. CI builds and tests the core on Windows and Linux.

## Research commands

```sh
ps5emu inspect sample.elf
ps5emu prepare sample.elf 0x500000
```

`inspect` displays load segments, libraries, and imports. `prepare` stages a
single ELF image and applies relocations; missing required imports produce an
error. These commands do not execute guest instructions. See
[runtime details](docs/RUNTIME.md) for limitations and resolver integration.

## Legal

This project does not include or distribute copyrighted firmware, proprietary system libraries, cryptographic keys, or DRM-bypass material. Users are responsible for using software they are legally entitled to use.
