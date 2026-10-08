#include <ps5emu/runtime/NativeLeafExecutor.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#elif defined(__linux__)
#include <sys/auxv.h>
#endif

namespace ps5emu::runtime {
namespace {

#if !defined(_M_X64) && !defined(__x86_64__)
#error "NativeLeafExecutor requires an x86-64 host"
#endif

constexpr std::size_t kCodeSize = 4096;
constexpr std::size_t kReturnStubOffset = 512;
constexpr std::uint32_t kFxSaveStackSize = 0x208;

enum class Register : std::uint8_t {
    Rax = 0,
    Rcx = 1,
    Rdx = 2,
    Rbx = 3,
    Rsp = 4,
    Rbp = 5,
    Rsi = 6,
    Rdi = 7,
    R8 = 8,
    R9 = 9,
    R10 = 10,
    R11 = 11,
    R12 = 12,
    R13 = 13,
    R14 = 14,
    R15 = 15,
};

std::uint8_t RegisterId(Register value) {
    return static_cast<std::uint8_t>(value);
}

class X64Emitter final {
public:
    void Push(Register reg) {
        const auto id = RegisterId(reg);
        if (id >= 8) {
            Byte(0x41);
        }
        Byte(static_cast<std::uint8_t>(
            0x50 + (id & 7u)));
    }

    void Pop(Register reg) {
        const auto id = RegisterId(reg);
        if (id >= 8) {
            Byte(0x41);
        }
        Byte(static_cast<std::uint8_t>(
            0x58 + (id & 7u)));
    }

    void PushFq() {
        Byte(0x9c);
    }

    void PopFq() {
        Byte(0x9d);
    }

    void SubRspImm32(std::uint32_t value) {
        Byte(0x48);
        Byte(0x81);
        Byte(0xec);
        U32(value);
    }

    void AddRspImm32(std::uint32_t value) {
        Byte(0x48);
        Byte(0x81);
        Byte(0xc4);
        U32(value);
    }

    void SubRegImm8(Register reg,
                    std::uint8_t value) {
        const auto id = RegisterId(reg);
        std::uint8_t rex = 0x48;
        if (id >= 8) {
            rex |= 0x01;
        }

        Byte(rex);
        Byte(0x83);
        Byte(static_cast<std::uint8_t>(
            0xe8 | (id & 7u)));
        Byte(value);
    }

    void MovRegImm64(Register reg,
                     std::uint64_t value) {
        const auto id = RegisterId(reg);
        std::uint8_t rex = 0x48;
        if (id >= 8) {
            rex |= 0x01;
        }

        Byte(rex);
        Byte(static_cast<std::uint8_t>(
            0xb8 + (id & 7u)));
        U64(value);
    }

    void MovRegReg(Register destination,
                   Register source) {
        const auto dst = RegisterId(destination);
        const auto src = RegisterId(source);

        std::uint8_t rex = 0x48;
        if (src >= 8) {
            rex |= 0x04;
        }
        if (dst >= 8) {
            rex |= 0x01;
        }

        Byte(rex);
        Byte(0x89);
        Byte(static_cast<std::uint8_t>(
            0xc0 |
            ((src & 7u) << 3u) |
            (dst & 7u)));
    }

    void MovRegMemDisp32(Register destination,
                         Register base,
                         std::int32_t displacement) {
        EmitMemoryMove(
            0x8b,
            destination,
            base,
            displacement);
    }

    void MovMemDisp32Reg(Register base,
                         std::int32_t displacement,
                         Register source) {
        EmitMemoryMove(
            0x89,
            source,
            base,
            displacement);
    }

    void MovAbsRaxStore(std::uint64_t address) {
        Byte(0x48);
        Byte(0xa3);
        U64(address);
    }

    void FxSave64Rsp() {
        Byte(0x48);
        Byte(0x0f);
        Byte(0xae);
        Byte(0x04);
        Byte(0x24);
    }

    void FxRstor64Rsp() {
        Byte(0x48);
        Byte(0x0f);
        Byte(0xae);
        Byte(0x0c);
        Byte(0x24);
    }

    void TestRegReg(Register reg) {
        const auto id = RegisterId(reg);

        std::uint8_t rex = 0x48;
        if (id >= 8) {
            rex |= 0x05;
        }

        Byte(rex);
        Byte(0x85);
        Byte(static_cast<std::uint8_t>(
            0xc0 |
            ((id & 7u) << 3u) |
            (id & 7u)));
    }

    [[nodiscard]] std::size_t JzRel32() {
        Byte(0x0f);
        Byte(0x84);
        const auto displacementOffset = bytes_.size();
        U32(0);
        return displacementOffset;
    }

    [[nodiscard]] std::size_t Position() const noexcept {
        return bytes_.size();
    }

    void PatchRel32(std::size_t displacementOffset,
                    std::size_t targetOffset) {
        if (displacementOffset + 4 > bytes_.size()) {
            throw std::out_of_range(
                "Native trampoline branch patch is outside emitted code");
        }

        const auto nextInstruction =
            displacementOffset + 4;

        const auto delta =
            static_cast<std::int64_t>(targetOffset) -
            static_cast<std::int64_t>(nextInstruction);

        if (delta <
                std::numeric_limits<std::int32_t>::min() ||
            delta >
                std::numeric_limits<std::int32_t>::max()) {
            throw std::overflow_error(
                "Native trampoline branch exceeds rel32 range");
        }

        const auto encoded =
            static_cast<std::uint32_t>(
                static_cast<std::int32_t>(delta));

        for (unsigned shift = 0;
             shift < 32;
             shift += 8) {
            bytes_[displacementOffset + shift / 8] =
                static_cast<std::byte>(
                    encoded >> shift);
        }
    }

    void ReadFsBase(Register reg) {
        EmitFsBase(0, reg);
    }

    void WriteFsBase(Register reg) {
        EmitFsBase(2, reg);
    }

    void Ret() {
        Byte(0xc3);
    }

    void PadTo(std::size_t size,
               std::byte value) {
        if (bytes_.size() > size) {
            throw std::runtime_error(
                "Native leaf trampoline entry exceeds its reserved slot");
        }

        bytes_.resize(size, value);
    }

    [[nodiscard]] const std::vector<std::byte>&
    Bytes() const noexcept {
        return bytes_;
    }

private:
    void EmitFsBase(std::uint8_t extension,
                    Register reg) {
        const auto id = RegisterId(reg);

        Byte(0xf3);

        std::uint8_t rex = 0x48;
        if (id >= 8) {
            rex |= 0x01;
        }

        Byte(rex);
        Byte(0x0f);
        Byte(0xae);
        Byte(static_cast<std::uint8_t>(
            0xc0 |
            ((extension & 7u) << 3u) |
            (id & 7u)));
    }

    void EmitMemoryMove(std::uint8_t opcode,
                        Register regField,
                        Register base,
                        std::int32_t displacement) {
        const auto reg = RegisterId(regField);
        const auto rm = RegisterId(base);

        if ((rm & 7u) == 4u) {
            throw std::logic_error(
                "Emitter memory helper does not support an RSP/R12 base");
        }

        std::uint8_t rex = 0x48;
        if (reg >= 8) {
            rex |= 0x04;
        }
        if (rm >= 8) {
            rex |= 0x01;
        }

        Byte(rex);
        Byte(opcode);
        Byte(static_cast<std::uint8_t>(
            0x80 |
            ((reg & 7u) << 3u) |
            (rm & 7u)));
        U32(static_cast<std::uint32_t>(
            displacement));
    }

    void Byte(std::uint8_t value) {
        bytes_.push_back(
            static_cast<std::byte>(value));
    }

    void U32(std::uint32_t value) {
        for (unsigned shift = 0;
             shift < 32;
             shift += 8) {
            Byte(static_cast<std::uint8_t>(
                value >> shift));
        }
    }

    void U64(std::uint64_t value) {
        for (unsigned shift = 0;
             shift < 64;
             shift += 8) {
            Byte(static_cast<std::uint8_t>(
                value >> shift));
        }
    }

    std::vector<std::byte> bytes_;
};

bool GuestFsBaseAvailable() noexcept {
#if defined(_WIN32)
#ifdef PF_RDWRFSGSBASE_AVAILABLE
    return IsProcessorFeaturePresent(
        PF_RDWRFSGSBASE_AVAILABLE) != 0;
#else
    return false;
#endif
#elif defined(__linux__)
#ifdef AT_HWCAP2
    constexpr unsigned long kHwcap2Fsgsbase =
        1ul << 1u;
    return (getauxval(AT_HWCAP2) &
            kHwcap2Fsgsbase) != 0;
#else
    return false;
#endif
#endif
}

std::int32_t CheckedDisplacement(
    std::size_t value) {
    if (value >
        static_cast<std::size_t>(
            std::numeric_limits<std::int32_t>::max())) {
        throw std::overflow_error(
            "Native leaf trampoline state offset exceeds disp32");
    }

    return static_cast<std::int32_t>(value);
}

} // namespace

bool NativeLeafExecutor::SupportsGuestFsBase() noexcept {
    return GuestFsBaseAvailable();
}

NativeLeafExecutor::NativeLeafExecutor()
    : code_(
          NativeMemoryRegion::Allocate(
              kCodeSize)) {
    BuildTrampoline();
}

std::uint64_t
NativeLeafExecutor::EscapeAddress() const noexcept {
    return static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(
            code_.Data()) +
        kReturnStubOffset);
}

void NativeLeafExecutor::Run(
    SysvGuestContext& context,
    const NativeImage& nativeImage) {
    if (context.fsBase != 0) {
        if (!SupportsGuestFsBase()) {
            throw std::runtime_error(
                "Host OS does not expose user-mode FSGSBASE");
        }

        const auto tlsMapping =
            nativeImage.FindMapping(
                context.fsBase,
                sizeof(std::uint64_t));

        if (!tlsMapping.has_value() ||
            !memory::HasProtection(
                tlsMapping->protection,
                memory::Protection::Read)) {
            throw std::runtime_error(
                "Guest FS base is not backed by readable native memory");
        }
    }

    const auto codeMapping =
        nativeImage.FindMapping(
            context.rip,
            1);

    if (!codeMapping.has_value() ||
        !memory::HasProtection(
            codeMapping->protection,
            memory::Protection::Execute)) {
        throw std::runtime_error(
            "Guest RIP is not backed by executable native memory");
    }

    if (context.rsp < 16) {
        throw std::runtime_error(
            "Guest stack pointer cannot provide a synthetic return frame");
    }

    const auto stackMapping =
        nativeImage.FindMapping(
            context.rsp - 16,
            16);

    if (!stackMapping.has_value() ||
        !memory::HasProtection(
            stackMapping->protection,
            memory::Protection::Write)) {
        throw std::runtime_error(
            "Guest stack is not backed by writable native memory");
    }

    state_.input = context;
    state_.output = context;
    state_.hostRsp = 0;
    state_.hostRflags = 0;
    state_.hostFsBase = 0;
    state_.savedGuestRax = 0;

    using Trampoline = void (*)();
    const auto trampoline =
        reinterpret_cast<Trampoline>(
            code_.Data());

    trampoline();

    state_.output.rip = 0;
    state_.output.fsBase = context.fsBase;
    context = state_.output;
}

void NativeLeafExecutor::Resume(
    SysvGuestContext& context,
    const NativeImage& nativeImage) {
    if (context.rsp >
        std::numeric_limits<std::uint64_t>::max() -
            sizeof(std::uint64_t)) {
        throw std::runtime_error(
            "Guest stack pointer overflows while preparing native resume");
    }

    auto resumed = context;
    resumed.rsp += sizeof(std::uint64_t);

    Run(
        resumed,
        nativeImage);

    context = resumed;
}

void NativeLeafExecutor::BuildTrampoline() {
    static_assert(
        std::is_standard_layout_v<State>);
    static_assert(
        std::is_standard_layout_v<SysvGuestContext>);

    const auto stateAddress =
        reinterpret_cast<std::uintptr_t>(
            &state_);
    const auto codeAddress =
        reinterpret_cast<std::uintptr_t>(
            code_.Data());
    const auto returnStubAddress =
        codeAddress + kReturnStubOffset;

    const auto inputOffset =
        [](std::size_t fieldOffset) {
            return CheckedDisplacement(
                offsetof(State, input) +
                fieldOffset);
        };

    const auto outputOffset =
        [](std::size_t fieldOffset) {
            return CheckedDisplacement(
                offsetof(State, output) +
                fieldOffset);
        };

    const auto stateOffset =
        [](std::size_t fieldOffset) {
            return CheckedDisplacement(
                fieldOffset);
        };

    X64Emitter emitter;

    emitter.Push(Register::Rbx);
    emitter.Push(Register::Rbp);
    emitter.Push(Register::Rdi);
    emitter.Push(Register::Rsi);
    emitter.Push(Register::R12);
    emitter.Push(Register::R13);
    emitter.Push(Register::R14);
    emitter.Push(Register::R15);

    emitter.MovRegImm64(
        Register::R11,
        stateAddress);

    emitter.PushFq();
    emitter.Pop(Register::R10);
    emitter.MovMemDisp32Reg(
        Register::R11,
        stateOffset(
            offsetof(State, hostRflags)),
        Register::R10);

    emitter.SubRspImm32(
        kFxSaveStackSize);
    emitter.FxSave64Rsp();

    emitter.MovMemDisp32Reg(
        Register::R11,
        stateOffset(
            offsetof(State, hostRsp)),
        Register::Rsp);

    emitter.MovRegMemDisp32(
        Register::R10,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, fsBase)));
    emitter.TestRegReg(Register::R10);
    const auto skipGuestFsSetup =
        emitter.JzRel32();

    emitter.ReadFsBase(Register::R10);
    emitter.MovMemDisp32Reg(
        Register::R11,
        stateOffset(
            offsetof(State, hostFsBase)),
        Register::R10);
    emitter.MovRegMemDisp32(
        Register::R10,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, fsBase)));
    emitter.WriteFsBase(Register::R10);

    emitter.PatchRel32(
        skipGuestFsSetup,
        emitter.Position());

    emitter.MovRegMemDisp32(
        Register::Rax,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, rsp)));
    emitter.SubRegImm8(
        Register::Rax,
        16);

    emitter.MovRegMemDisp32(
        Register::Rcx,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, rip)));
    emitter.MovMemDisp32Reg(
        Register::Rax,
        0,
        Register::Rcx);

    emitter.MovRegImm64(
        Register::Rcx,
        returnStubAddress);
    emitter.MovMemDisp32Reg(
        Register::Rax,
        8,
        Register::Rcx);

    emitter.MovRegMemDisp32(
        Register::R10,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, rflags)));
    emitter.Push(Register::R10);
    emitter.PopFq();

    emitter.MovRegReg(
        Register::Rsp,
        Register::Rax);

    emitter.MovRegMemDisp32(
        Register::Rdi,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, rdi)));
    emitter.MovRegMemDisp32(
        Register::Rsi,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, rsi)));
    emitter.MovRegMemDisp32(
        Register::Rdx,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, rdx)));
    emitter.MovRegMemDisp32(
        Register::Rcx,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, rcx)));
    emitter.MovRegMemDisp32(
        Register::R8,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, r8)));
    emitter.MovRegMemDisp32(
        Register::R9,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, r9)));
    emitter.MovRegMemDisp32(
        Register::Rax,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, rax)));
    emitter.MovRegMemDisp32(
        Register::Rbx,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, rbx)));
    emitter.MovRegMemDisp32(
        Register::Rbp,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, rbp)));
    emitter.MovRegMemDisp32(
        Register::R10,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, r10)));
    emitter.MovRegMemDisp32(
        Register::R12,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, r12)));
    emitter.MovRegMemDisp32(
        Register::R13,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, r13)));
    emitter.MovRegMemDisp32(
        Register::R14,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, r14)));
    emitter.MovRegMemDisp32(
        Register::R15,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, r15)));

    emitter.MovRegMemDisp32(
        Register::R11,
        Register::R11,
        inputOffset(
            offsetof(SysvGuestContext, r11)));

    emitter.Ret();

    emitter.PadTo(
        kReturnStubOffset,
        std::byte{0xcc});

    emitter.MovAbsRaxStore(
        stateAddress +
        offsetof(State, savedGuestRax));

    emitter.MovRegImm64(
        Register::Rax,
        stateAddress);

    emitter.MovMemDisp32Reg(
        Register::Rax,
        outputOffset(
            offsetof(SysvGuestContext, r10)),
        Register::R10);

    emitter.MovRegMemDisp32(
        Register::R10,
        Register::Rax,
        inputOffset(
            offsetof(SysvGuestContext, fsBase)));
    emitter.TestRegReg(Register::R10);
    const auto skipHostFsRestore =
        emitter.JzRel32();

    emitter.MovRegMemDisp32(
        Register::R10,
        Register::Rax,
        stateOffset(
            offsetof(State, hostFsBase)));
    emitter.WriteFsBase(Register::R10);

    emitter.PatchRel32(
        skipHostFsRestore,
        emitter.Position());

    emitter.MovMemDisp32Reg(
        Register::Rax,
        outputOffset(
            offsetof(SysvGuestContext, rdi)),
        Register::Rdi);
    emitter.MovMemDisp32Reg(
        Register::Rax,
        outputOffset(
            offsetof(SysvGuestContext, rsi)),
        Register::Rsi);
    emitter.MovMemDisp32Reg(
        Register::Rax,
        outputOffset(
            offsetof(SysvGuestContext, rdx)),
        Register::Rdx);
    emitter.MovMemDisp32Reg(
        Register::Rax,
        outputOffset(
            offsetof(SysvGuestContext, rcx)),
        Register::Rcx);
    emitter.MovMemDisp32Reg(
        Register::Rax,
        outputOffset(
            offsetof(SysvGuestContext, r8)),
        Register::R8);
    emitter.MovMemDisp32Reg(
        Register::Rax,
        outputOffset(
            offsetof(SysvGuestContext, r9)),
        Register::R9);
    emitter.MovMemDisp32Reg(
        Register::Rax,
        outputOffset(
            offsetof(SysvGuestContext, rsp)),
        Register::Rsp);
    emitter.MovMemDisp32Reg(
        Register::Rax,
        outputOffset(
            offsetof(SysvGuestContext, rbx)),
        Register::Rbx);
    emitter.MovMemDisp32Reg(
        Register::Rax,
        outputOffset(
            offsetof(SysvGuestContext, rbp)),
        Register::Rbp);
    emitter.MovMemDisp32Reg(
        Register::Rax,
        outputOffset(
            offsetof(SysvGuestContext, r11)),
        Register::R11);
    emitter.MovMemDisp32Reg(
        Register::Rax,
        outputOffset(
            offsetof(SysvGuestContext, r12)),
        Register::R12);
    emitter.MovMemDisp32Reg(
        Register::Rax,
        outputOffset(
            offsetof(SysvGuestContext, r13)),
        Register::R13);
    emitter.MovMemDisp32Reg(
        Register::Rax,
        outputOffset(
            offsetof(SysvGuestContext, r14)),
        Register::R14);
    emitter.MovMemDisp32Reg(
        Register::Rax,
        outputOffset(
            offsetof(SysvGuestContext, r15)),
        Register::R15);

    emitter.PushFq();
    emitter.Pop(Register::R11);
    emitter.MovMemDisp32Reg(
        Register::Rax,
        outputOffset(
            offsetof(SysvGuestContext, rflags)),
        Register::R11);

    emitter.MovRegMemDisp32(
        Register::R11,
        Register::Rax,
        stateOffset(
            offsetof(State, savedGuestRax)));
    emitter.MovMemDisp32Reg(
        Register::Rax,
        outputOffset(
            offsetof(SysvGuestContext, rax)),
        Register::R11);

    emitter.MovRegMemDisp32(
        Register::Rsp,
        Register::Rax,
        stateOffset(
            offsetof(State, hostRsp)));

    emitter.FxRstor64Rsp();
    emitter.AddRspImm32(
        kFxSaveStackSize);

    emitter.Pop(Register::R15);
    emitter.Pop(Register::R14);
    emitter.Pop(Register::R13);
    emitter.Pop(Register::R12);
    emitter.Pop(Register::Rsi);
    emitter.Pop(Register::Rdi);
    emitter.Pop(Register::Rbp);
    emitter.Pop(Register::Rbx);

    emitter.MovRegMemDisp32(
        Register::R10,
        Register::Rax,
        stateOffset(
            offsetof(State, hostRflags)));
    emitter.Push(Register::R10);
    emitter.PopFq();

    emitter.Ret();

    if (emitter.Bytes().size() >
        code_.Size()) {
        throw std::runtime_error(
            "Native leaf trampoline exceeds its code region");
    }

    code_.Write(
        0,
        emitter.Bytes());

    code_.Protect(
        memory::Protection::Read |
        memory::Protection::Execute);
}

} // namespace ps5emu::runtime
