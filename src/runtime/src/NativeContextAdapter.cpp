#include <ps5emu/runtime/NativeContextAdapter.hpp>

namespace ps5emu::runtime {

#if defined(_WIN32)

SysvGuestContext NativeContextAdapter::FromNative(
    const CONTEXT& nativeContext,
    std::uint64_t fsBase) noexcept {
    return SysvGuestContext{
        .rdi = nativeContext.Rdi,
        .rsi = nativeContext.Rsi,
        .rdx = nativeContext.Rdx,
        .rcx = nativeContext.Rcx,
        .r8 = nativeContext.R8,
        .r9 = nativeContext.R9,
        .rsp = nativeContext.Rsp,
        .rax = nativeContext.Rax,
        .rip = nativeContext.Rip,
        .fsBase = fsBase,
    };
}

void NativeContextAdapter::ToNative(
    const SysvGuestContext& guestContext,
    CONTEXT& nativeContext) noexcept {
    nativeContext.Rdi = guestContext.rdi;
    nativeContext.Rsi = guestContext.rsi;
    nativeContext.Rdx = guestContext.rdx;
    nativeContext.Rcx = guestContext.rcx;
    nativeContext.R8 = guestContext.r8;
    nativeContext.R9 = guestContext.r9;
    nativeContext.Rsp = guestContext.rsp;
    nativeContext.Rax = guestContext.rax;
    nativeContext.Rip = guestContext.rip;
}

#elif defined(__linux__)

SysvGuestContext NativeContextAdapter::FromNative(
    const ucontext_t& nativeContext,
    std::uint64_t fsBase) noexcept {
    const auto& registers = nativeContext.uc_mcontext.gregs;

    return SysvGuestContext{
        .rdi = static_cast<std::uint64_t>(registers[REG_RDI]),
        .rsi = static_cast<std::uint64_t>(registers[REG_RSI]),
        .rdx = static_cast<std::uint64_t>(registers[REG_RDX]),
        .rcx = static_cast<std::uint64_t>(registers[REG_RCX]),
        .r8 = static_cast<std::uint64_t>(registers[REG_R8]),
        .r9 = static_cast<std::uint64_t>(registers[REG_R9]),
        .rsp = static_cast<std::uint64_t>(registers[REG_RSP]),
        .rax = static_cast<std::uint64_t>(registers[REG_RAX]),
        .rip = static_cast<std::uint64_t>(registers[REG_RIP]),
        .fsBase = fsBase,
    };
}

void NativeContextAdapter::ToNative(
    const SysvGuestContext& guestContext,
    ucontext_t& nativeContext) noexcept {
    auto& registers = nativeContext.uc_mcontext.gregs;

    registers[REG_RDI] = static_cast<greg_t>(guestContext.rdi);
    registers[REG_RSI] = static_cast<greg_t>(guestContext.rsi);
    registers[REG_RDX] = static_cast<greg_t>(guestContext.rdx);
    registers[REG_RCX] = static_cast<greg_t>(guestContext.rcx);
    registers[REG_R8] = static_cast<greg_t>(guestContext.r8);
    registers[REG_R9] = static_cast<greg_t>(guestContext.r9);
    registers[REG_RSP] = static_cast<greg_t>(guestContext.rsp);
    registers[REG_RAX] = static_cast<greg_t>(guestContext.rax);
    registers[REG_RIP] = static_cast<greg_t>(guestContext.rip);
}

#endif

} // namespace ps5emu::runtime
