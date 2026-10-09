#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <span>
#include <unordered_map>

#include <ps5emu/hle/GuestFileSystemAccess.hpp>

namespace ps5emu::runtime {

class SandboxFileSystem final
    : public hle::GuestFileSystemAccess {
public:
    explicit SandboxFileSystem(
        std::filesystem::path root);

    SandboxFileSystem(
        const SandboxFileSystem&) = delete;
    SandboxFileSystem& operator=(
        const SandboxFileSystem&) = delete;
    SandboxFileSystem(
        SandboxFileSystem&&) = delete;
    SandboxFileSystem& operator=(
        SandboxFileSystem&&) = delete;

    ~SandboxFileSystem() override;

    [[nodiscard]] hle::GuestFileResult
    Open(std::string_view path,
         std::int32_t flags,
         std::uint16_t mode) override;

    [[nodiscard]] hle::GuestFileResult
    Close(std::int32_t descriptor) override;

    [[nodiscard]] hle::GuestFileResult
    Read(std::int32_t descriptor,
         std::span<std::byte> output) override;

    [[nodiscard]] hle::GuestFileResult
    Write(std::int32_t descriptor,
          std::span<const std::byte> input) override;

    [[nodiscard]] hle::GuestFileResult
    Seek(std::int32_t descriptor,
         std::int64_t offset,
         std::int32_t whence) override;

    [[nodiscard]] const std::filesystem::path&
    Root() const noexcept;

private:
    struct ResolvedPath {
        std::filesystem::path nativePath;
        std::uint64_t errorCode = 0;
    };

    [[nodiscard]] ResolvedPath
    ResolvePath(
        std::string_view guestPath,
        bool allowMissingLeaf) const;

    [[nodiscard]] std::int32_t
    AllocateDescriptorLocked();

    [[nodiscard]] static std::uint64_t
    ErrorFromErrno(int error) noexcept;

    [[nodiscard]] static hle::GuestFileResult
    Failure(int error) noexcept;

    [[nodiscard]] static int
    MapOpenFlags(std::int32_t guestFlags);

    static void
    NativeCloseNoThrow(int descriptor) noexcept;

    std::filesystem::path root_;

    mutable std::mutex mutex_;
    std::unordered_map<std::int32_t, int>
        descriptors_;
    std::int32_t nextDescriptor_ = 3;
};

} // namespace ps5emu::runtime
