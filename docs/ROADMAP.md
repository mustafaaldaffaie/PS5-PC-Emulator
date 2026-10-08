# Roadmap

## Phase 1 - Foundation

- [x] Repository bootstrap
- [x] CMake project structure
- [x] Optional Qt frontend
- [x] English and Arabic localization foundation
- [x] Source-language guard
- [x] Initial ELF64 parser
- [x] Initial guest-memory abstraction
- [x] Unit tests for ELF and memory
- [ ] Structured logging
- [ ] Crash reporting

## Phase 2 - Executable loading

- [x] Map PT_LOAD segments
- [x] Entry-point validation
- [x] Transactional loading with explicit load bias
- [x] Dynamic metadata and DT_NEEDED parsing
- [x] SCE dynamic data and DT_SCE table layout
- [x] Relocation model
- [x] Dynamic symbol parsing
- [x] Import table parsing
- [x] Single-image linking with local and injected external addresses
- [x] SCE module and library metadata
- [ ] SELF container research and clean-room parser

## Phase 3 - HLE

- [x] Qualified SCE import identity decoding
- [x] NID name database and forward hashing
- [x] HLE service registry foundation
- [x] Synthetic guest thunk allocation and relocation binding
- [x] Guest-call ABI dispatch (integer/pointer baseline)
- [x] HLE guest-memory access interface
- [ ] Threading primitives
- [ ] Time and synchronization
- [ ] Filesystem
- [x] Basic libc memory bridge (memcpy/memmove/memset)
- [ ] Additional libc coverage
- [ ] Module loader

## Phase 4 - Native execution

- [x] Guest stack mapping
- [x] PT_TLS parsing and per-thread TLS block initialization
- [x] TLS runtime ABI and FS-base integration
- [x] HLE INT3 trap/return control-flow baseline
- [x] Native Windows/Linux exception hookup
- [x] Native guest/HLE execution and resume loop
- [ ] Host-call trampolines beyond trap slots
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
