# Architecture

## Direction

PS5-PC-Emulator is organized as a clean-room compatibility project.

The initial architecture is split into:

- Core: platform-independent emulator and compatibility logic.
- App: command-line launcher used for bring-up and diagnostics.
- Frontend: optional Qt desktop UI.
- Locales: external translation resources.
- Tools: validation and repository utilities.

## Initial milestones

1. Build system and CI.
2. Guest executable inspection and ELF64 loading.
3. Guest memory model.
4. Import and NID resolution.
5. HLE service layer.
6. Native x86-64 execution experiments.
7. Vulkan graphics translation.
8. Audio, input, filesystem, and save-data services.
9. Game library and per-title configuration.

## Localization policy

Source code, identifiers, comments, logs, and build scripts remain English-only.
Localized user-facing text is stored in translation resources.
