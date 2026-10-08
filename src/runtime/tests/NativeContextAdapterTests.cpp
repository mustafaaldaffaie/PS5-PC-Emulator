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

    auto changed = guest;
    changed.rdi = 11;
    changed.rsi = 12;
    changed.rdx = 13;
    changed.rcx = 14;
    changed.r8 = 15;
    changed.r9 = 16;
    changed.rsp = 0x720000;
    changed.rax = 17;
    changed.rip = 0x500123;
    changed.fsBase = 0x730020;

    NativeContextAdapter::ToNative(changed, native);

    assert(native.Rdi == 11);
    assert(native.Rsi == 12);
    assert(native.Rdx == 13);
    assert(native.Rcx == 14);
    assert(native.R8 == 15);
    assert(native.R9 == 16);
    assert(native.Rsp == 0x720000);
    assert(native.Rax == 17);
    assert(native.Rip == 0x500123);
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

    auto changed = guest;
    changed.rdi = 11;
    changed.rsi = 12;
    changed.rdx = 13;
    changed.rcx = 14;
    changed.r8 = 15;
    changed.r9 = 16;
    changed.rsp = 0x720000;
    changed.rax = 17;
    changed.rip = 0x500123;
    changed.fsBase = 0x730020;

    NativeContextAdapter::ToNative(changed, native);

    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_RDI]) == 11);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_RSI]) == 12);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_RDX]) == 13);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_RCX]) == 14);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_R8]) == 15);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_R9]) == 16);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_RSP]) == 0x720000);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_RAX]) == 17);
    assert(static_cast<std::uint64_t>(
        native.uc_mcontext.gregs[REG_RIP]) == 0x500123);
#endif

    return 0;
}
