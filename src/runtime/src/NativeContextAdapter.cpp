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
        .rbx = nativeContext.Rbx,
        .rbp = nativeContext.Rbp,
        .r10 = nativeContext.R10,
        .r11 = nativeContext.R11,
        .r12 = nativeContext.R12,
        .r13 = nativeContext.R13,
        .r14 = nativeContext.R14,
        .r15 = nativeContext.R15,
        .rflags = nativeContext.EFlags,
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
    nativeContext.Rbx = guestContext.rbx;
    nativeContext.Rbp = guestContext.rbp;
    nativeContext.R10 = guestContext.r10;
    nativeContext.R11 = guestContext.r11;
    nativeContext.R12 = guestContext.r12;
    nativeContext.R13 = guestContext.r13;
    nativeContext.R14 = guestContext.r14;
    nativeContext.R15 = guestContext.r15;
    nativeContext.EFlags =
        static_cast<DWORD>(
            guestContext.rflags);
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
        .rbx = static_cast<std::uint64_t>(registers[REG_RBX]),
        .rbp = static_cast<std::uint64_t>(registers[REG_RBP]),
        .r10 = static_cast<std::uint64_t>(registers[REG_R10]),
        .r11 = static_cast<std::uint64_t>(registers[REG_R11]),
        .r12 = static_cast<std::uint64_t>(registers[REG_R12]),
        .r13 = static_cast<std::uint64_t>(registers[REG_R13]),
        .r14 = static_cast<std::uint64_t>(registers[REG_R14]),
        .r15 = static_cast<std::uint64_t>(registers[REG_R15]),
        .rflags = static_cast<std::uint64_t>(registers[REG_EFL]),
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
    registers[REG_RBX] = static_cast<greg_t>(guestContext.rbx);
    registers[REG_RBP] = static_cast<greg_t>(guestContext.rbp);
    registers[REG_R10] = static_cast<greg_t>(guestContext.r10);
    registers[REG_R11] = static_cast<greg_t>(guestContext.r11);
    registers[REG_R12] = static_cast<greg_t>(guestContext.r12);
    registers[REG_R13] = static_cast<greg_t>(guestContext.r13);
    registers[REG_R14] = static_cast<greg_t>(guestContext.r14);
    registers[REG_R15] = static_cast<greg_t>(guestContext.r15);
    registers[REG_EFL] = static_cast<greg_t>(guestContext.rflags);
}

#endif

} // namespace ps5emu::runtime
