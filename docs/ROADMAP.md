# Roadmap

## Phase 1 - Foundation

- [x] Repository bootstrap
- [x] CMake project structure
- [x] Optional Qt frontend
- [x] English and Arabic localization foundation
- [x] Source-language guard
- [x] Initial ELF64 parser
- [x] Initial guest-memory abstraction
- [ ] Unit tests for ELF and memory
- [ ] Structured logging
- [ ] Crash reporting

## Phase 2 - Executable loading

- [ ] Map PT_LOAD segments
- [ ] Entry-point validation
- [ ] Relocation model
- [ ] Dynamic symbol parsing
- [ ] Import table parsing
- [ ] Module metadata
- [ ] SELF container research and clean-room parser

## Phase 3 - HLE

- [ ] NID resolver
- [ ] Kernel service registry
- [ ] Threading primitives
- [ ] Time and synchronization
- [ ] Filesystem
- [ ] Basic libc bridge
- [ ] Module loader

## Phase 4 - Native execution

- [ ] Guest stack and TLS
- [ ] Host-call trampolines
- [ ] Exception handling
- [ ] Syscall interception strategy
- [ ] AMD-only instruction handling
- [ ] Intel compatibility lowering

## Phase 5 - Graphics

- [ ] Vulkan device layer
- [ ] Command-stream abstraction
- [ ] PM4 research
- [ ] Shader decoder
- [ ] Shader IR
- [ ] SPIR-V backend
- [ ] Pipeline cache
- [ ] Texture and render-target handling

## Phase 6 - Platform services

- [ ] SDL input
- [ ] Audio backend
- [ ] Save-data service
- [ ] User service
- [ ] Trophy stubs
- [ ] Networking stubs

## Phase 7 - Frontend

- [ ] Game library
- [ ] Add-game workflow
- [ ] Per-title settings
- [ ] Vulkan device selection
- [ ] Controller settings
- [ ] Logs and diagnostics
- [ ] Compatibility status
- [ ] Packaging and updater
