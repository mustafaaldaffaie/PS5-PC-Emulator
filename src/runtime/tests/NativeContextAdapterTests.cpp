#include <ps5emu/runtime/NativeContextAdapter.hpp>

#include <cassert>
#include <cstdint>

int main() {
    using ps5emu::runtime::NativeContextAdapter;

#if defined(_WIN32)
    CONTEXT native{};
    native.Rdi = 1;
    native.Rsi = 2;
    native.Rdx = 3;
    native.Rcx = 4;
    native.R8 = 5;
    native.R9 = 6;
    native.Rsp = 0x700000;
    native.Rax = 7;
    native.Rip = 0x400123;
    native.Rbx = 8;
    native.Rbp = 9;
    native.R10 = 10;
    native.R11 = 11;
    native.R12 = 12;
    native.R13 = 13;
    native.R14 = 14;
    native.R15 = 15;
    native.EFlags = 0x246;

    const auto guest =
        NativeContextAdapter::FromNative(
            native,
            0x710020);

    assert(guest.rdi == 1);
    assert(guest.rsi == 2);
    assert(guest.rdx == 3);
    assert(guest.rcx == 4);
    assert(guest.r8 == 5);
    assert(guest.r9 == 6);
    assert(guest.rsp == 0x700000);
    assert(guest.rax == 7);
    assert(guest.rip == 0x400123);
    assert(guest.fsBase == 0x710020);
    assert(guest.rbx == 8);
    assert(guest.rbp == 9);
    assert(guest.r10 == 10);
    assert(guest.r11 == 11);
    assert(guest.r12 == 12);
    assert(guest.r13 == 13);
    assert(guest.r14 == 14);
    assert(guest.r15 == 15);
    assert(guest.rflags == 0x246);

    auto changed = guest;
    changed.rdi = 21;
    changed.rsi = 22;
    changed.rdx = 23;
    changed.rcx = 24;
    changed.r8 = 25;
    changed.r9 = 26;
    changed.rsp = 0x720000;
    changed.rax = 27;
    changed.rip = 0x500123;
    changed.fsBase = 0x730020;
    changed.rbx = 28;
    changed.rbp = 29;
    changed.r10 = 30;
    changed.r11 = 31;
    changed.r12 = 32;
    changed.r13 = 33;
    changed.r14 = 34;
    changed.r15 = 35;
    changed.rflags = 0x202;

    NativeContextAdapter::ToNative(
        changed,
        native);

    assert(native.Rdi == 21);
    assert(native.Rsi == 22);
    assert(native.Rdx == 23);
    assert(native.Rcx == 24);
    assert(native.R8 == 25);
    assert(native.R9 == 26);
    assert(native.Rsp == 0x720000);
    assert(native.Rax == 27);
    assert(native.Rip == 0x500123);
    assert(native.Rbx == 28);
    assert(native.Rbp == 29);
    assert(native.R10 == 30);
    assert(native.R11 == 31);
    assert(native.R12 == 32);
    assert(native.R13 == 33);
    assert(native.R14 == 34);
    assert(native.R15 == 35);
    assert(native.EFlags == 0x202);
#elif defined(__linux__)
    ucontext_t native{};
    native.uc_mcontext.gregs[REG_RDI] = 1;
    native.uc_mcontext.gregs[REG_RSI] = 2;
    native.uc_mcontext.gregs[REG_RDX] = 3;
    native.uc_mcontext.gregs[REG_RCX] = 4;
    native.uc_mcontext.gregs[REG_R8] = 5;
    native.uc_mcontext.gregs[REG_R9] = 6;
    native.uc_mcontext.gregs[REG_RSP] = 0x700000;
    native.uc_mcontext.gregs[REG_RAX] = 7;
    native.uc_mcontext.gregs[REG_RIP] = 0x400123;
    native.uc_mcontext.gregs[REG_RBX] = 8;
    native.uc_mcontext.gregs[REG_RBP] = 9;
    native.uc_mcontext.gregs[REG_R10] = 10;
    native.uc_mcontext.gregs[REG_R11] = 11;
    native.uc_mcontext.gregs[REG_R12] = 12;
    native.uc_mcontext.gregs[REG_R13] = 13;
    native.uc_mcontext.gregs[REG_R14] = 14;
    native.uc_mcontext.gregs[REG_R15] = 15;
    native.uc_mcontext.gregs[REG_EFL] = 0x246;

    const auto guest =
        NativeContextAdapter::FromNative(
            native,
            0x710020);

    assert(guest.rdi == 1);
    assert(guest.rsi == 2);
    assert(guest.rdx == 3);
    assert(guest.rcx == 4);
    assert(guest.r8 == 5);
    assert(guest.r9 == 6);
    assert(guest.rsp == 0x700000);
    assert(guest.rax == 7);
    assert(guest.rip == 0x400123);
    assert(guest.fsBase == 0x710020);
    assert(guest.rbx == 8);
    assert(guest.rbp == 9);
    assert(guest.r10 == 10);
    assert(guest.r11 == 11);
    assert(guest.r12 == 12);
    assert(guest.r13 == 13);
    assert(guest.r14 == 14);
    assert(guest.r15 == 15);
    assert(guest.rflags == 0x246);

    auto changed = guest;
    changed.rdi = 21;
    changed.rsi = 22;
    changed.rdx = 23;
    changed.rcx = 24;
    changed.r8 = 25;
    changed.r9 = 26;
    changed.rsp = 0x720000;
    changed.rax = 27;
    changed.rip = 0x500123;
    changed.fsBase = 0x730020;
    changed.rbx = 28;
    changed.rbp = 29;
    changed.r10 = 30;
    changed.r11 = 31;
    changed.r12 = 32;
    changed.r13 = 33;
    changed.r14 = 34;
    changed.r15 = 35;
    changed.rflags = 0x202;

    NativeContextAdapter::ToNative(
        changed,
        native);

    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_RDI]) == 21);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_RSI]) == 22);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_RDX]) == 23);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_RCX]) == 24);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_R8]) == 25);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_R9]) == 26);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_RSP]) == 0x720000);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_RAX]) == 27);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_RIP]) == 0x500123);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_RBX]) == 28);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_RBP]) == 29);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_R10]) == 30);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_R11]) == 31);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_R12]) == 32);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_R13]) == 33);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_R14]) == 34);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_R15]) == 35);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_EFL]) == 0x202);
#endif

    return 0;
}
