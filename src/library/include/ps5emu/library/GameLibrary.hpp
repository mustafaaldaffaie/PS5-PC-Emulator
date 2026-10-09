#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace ps5emu::library {

struct GameEntry {
    std::filesystem::path executablePath;
    std::string displayName;
};

class GameLibrary final {
public:
    [[nodiscard]] bool
    Add(std::filesystem::path executablePath,
        std::string displayName = {});

    [[nodiscard]] bool
    Remove(const std::filesystem::path& executablePath);

    [[nodiscard]] const GameEntry*
    Find(const std::filesystem::path& executablePath) const noexcept;

    [[nodiscard]] const std::vector<GameEntry>&
    Entries() const noexcept;

    [[nodiscard]] std::size_t Size() const noexcept;
    [[nodiscard]] bool Empty() const noexcept;

    void Clear() noexcept;

    void Save(const std::filesystem::path& manifestPath) const;

    [[nodiscard]] static GameLibrary
    Load(const std::filesystem::path& manifestPath);

private:
    [[nodiscard]] static std::filesystem::path
    NormalizeExistingExecutable(
        const std::filesystem::path& executablePath);

    [[nodiscard]] static std::string
    DefaultDisplayName(
        const std::filesystem::path& executablePath);

    std::vector<GameEntry> entries_;
};

} // namespace ps5emu::library
