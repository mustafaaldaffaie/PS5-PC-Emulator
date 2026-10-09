#include <ps5emu/hle/KernelFile.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ps5emu::hle {
namespace {

constexpr std::uint64_t kSceErrorFault = 0x8002000eull;
constexpr std::uint64_t kSceErrorInvalid = 0x80020016ull;
constexpr std::uint64_t kSceErrorNotImplemented = 0x8002004eull;
constexpr std::size_t kMaximumGuestPath = 4096;

std::uint64_t EncodeResult(const GuestFileResult& result) {
    if (result.errorCode != 0) {
        return result.errorCode;
    }
    return std::bit_cast<std::uint64_t>(result.value);
}

std::int32_t ArgumentI32(std::uint64_t value) noexcept {
    return static_cast<std::int32_t>(
        static_cast<std::uint32_t>(value));
}

std::int64_t ArgumentI64(std::uint64_t value) noexcept {
    return std::bit_cast<std::int64_t>(value);
}

std::size_t CheckedSize(std::uint64_t value) {
    if (value > std::numeric_limits<std::size_t>::max()) {
        throw std::overflow_error(
            "Guest file I/O size is too large for this host");
    }
    return static_cast<std::size_t>(value);
}

bool ReadPath(HleCallFrame& frame,
              std::uint64_t address,
              std::string& path) {
    if (frame.memory == nullptr || address == 0) {
        return false;
    }

    path.clear();
    path.reserve(128);

    for (std::size_t index = 0;
         index < kMaximumGuestPath;
         ++index) {
        if (address >
            std::numeric_limits<std::uint64_t>::max() - index) {
            return false;
        }

        std::array<std::byte, 1> byte{};
        try {
            frame.memory->Read(address + index, byte);
        } catch (const std::exception&) {
            return false;
        }

        if (byte[0] == std::byte{0}) {
            return true;
        }

        path.push_back(
            static_cast<char>(
                std::to_integer<unsigned char>(byte[0])));
    }

    return false;
}

void OpenFile(HleCallFrame& frame) {
    if (frame.files == nullptr) {
        frame.returnValue = kSceErrorNotImplemented;
        return;
    }

    std::string path;
    if (!ReadPath(frame, frame.arguments[0], path)) {
        frame.returnValue = kSceErrorFault;
        return;
    }

    frame.returnValue = EncodeResult(
        frame.files->Open(
            path,
            ArgumentI32(frame.arguments[1]),
            static_cast<std::uint16_t>(
                frame.arguments[2])));
}

void CloseFile(HleCallFrame& frame) {
    if (frame.files == nullptr) {
        frame.returnValue = kSceErrorNotImplemented;
        return;
    }

    frame.returnValue = EncodeResult(
        frame.files->Close(
            ArgumentI32(frame.arguments[0])));
}

void ReadFile(HleCallFrame& frame) {
    if (frame.files == nullptr) {
        frame.returnValue = kSceErrorNotImplemented;
        return;
    }

    const auto size = CheckedSize(frame.arguments[2]);

    if (size != 0 &&
        (frame.memory == nullptr ||
         frame.arguments[1] == 0)) {
        frame.returnValue = kSceErrorFault;
        return;
    }

    std::vector<std::byte> buffer(size);
    const auto result =
        frame.files->Read(
            ArgumentI32(frame.arguments[0]),
            buffer);

    if (result.errorCode != 0) {
        frame.returnValue = result.errorCode;
        return;
    }

    if (result.value < 0 ||
        static_cast<std::uint64_t>(result.value) >
            buffer.size()) {
        frame.returnValue = kSceErrorInvalid;
        return;
    }

    const auto transferred =
        static_cast<std::size_t>(result.value);

    if (transferred != 0) {
        try {
            frame.memory->Write(
                frame.arguments[1],
                std::span<const std::byte>(
                    buffer.data(),
                    transferred));
        } catch (const std::exception&) {
            frame.returnValue = kSceErrorFault;
            return;
        }
    }

    frame.returnValue =
        static_cast<std::uint64_t>(transferred);
}

void WriteFile(HleCallFrame& frame) {
    if (frame.files == nullptr) {
        frame.returnValue = kSceErrorNotImplemented;
        return;
    }

    const auto size = CheckedSize(frame.arguments[2]);

    if (size != 0 &&
        (frame.memory == nullptr ||
         frame.arguments[1] == 0)) {
        frame.returnValue = kSceErrorFault;
        return;
    }

    std::vector<std::byte> buffer(size);

    if (size != 0) {
        try {
            frame.memory->Read(
                frame.arguments[1],
                buffer);
        } catch (const std::exception&) {
            frame.returnValue = kSceErrorFault;
            return;
        }
    }

    frame.returnValue = EncodeResult(
        frame.files->Write(
            ArgumentI32(frame.arguments[0]),
            buffer));
}

void SeekFile(HleCallFrame& frame) {
    if (frame.files == nullptr) {
        frame.returnValue = kSceErrorNotImplemented;
        return;
    }

    const auto whence =
        ArgumentI32(frame.arguments[2]);

    if (whence < 0 || whence > 2) {
        frame.returnValue = kSceErrorInvalid;
        return;
    }

    frame.returnValue = EncodeResult(
        frame.files->Seek(
            ArgumentI32(frame.arguments[0]),
            ArgumentI64(frame.arguments[1]),
            whence));
}

} // namespace

void KernelFile::Register(
    HleRegistry& registry,
    std::string module) {
    if (module.empty()) {
        throw std::invalid_argument(
            "Kernel file module name cannot be empty");
    }

    const auto moduleName = module;

    registry.RegisterSymbol(
        moduleName,
        "sceKernelOpen",
        OpenFile);
    registry.RegisterSymbol(
        moduleName,
        "sceKernelClose",
        CloseFile);
    registry.RegisterSymbol(
        moduleName,
        "sceKernelRead",
        ReadFile);
    registry.RegisterSymbol(
        moduleName,
        "sceKernelWrite",
        WriteFile);
    registry.RegisterSymbol(
        std::move(module),
        "sceKernelLseek",
        SeekFile);
}

} // namespace ps5emu::hle
