#include <ps5emu/library/GameLibrary.hpp>

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace ps5emu::library {
namespace {

constexpr std::string_view kManifestHeader =
    "PS5EMU_GAME_LIBRARY_V1";

std::string Escape(std::string_view value) {
    std::string output;
    output.reserve(value.size());

    for (const char character : value) {
        switch (character) {
        case '\\':
            output += "\\\\";
            break;
        case '\t':
            output += "\\t";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        default:
            output.push_back(character);
            break;
        }
    }

    return output;
}

std::string Unescape(std::string_view value) {
    std::string output;
    output.reserve(value.size());

    for (std::size_t index = 0;
         index < value.size();
         ++index) {
        const char character = value[index];

        if (character != '\\') {
            output.push_back(character);
            continue;
        }

        if (index + 1 >= value.size()) {
            throw std::runtime_error(
                "Game library manifest contains a dangling escape");
        }

        const char escaped = value[++index];

        switch (escaped) {
        case '\\':
            output.push_back('\\');
            break;
        case 't':
            output.push_back('\t');
            break;
        case 'n':
            output.push_back('\n');
            break;
        case 'r':
            output.push_back('\r');
            break;
        default:
            throw std::runtime_error(
                "Game library manifest contains an invalid escape");
        }
    }

    return output;
}

std::size_t FindUnescapedTab(std::string_view value) {
    bool escaped = false;

    for (std::size_t index = 0;
         index < value.size();
         ++index) {
        const char character = value[index];

        if (escaped) {
            escaped = false;
            continue;
        }

        if (character == '\\') {
            escaped = true;
            continue;
        }

        if (character == '\t') {
            return index;
        }
    }

    return std::string_view::npos;
}

std::string PathKey(const std::filesystem::path& path) {
    return path.generic_string();
}

} // namespace

bool GameLibrary::Add(
    std::filesystem::path executablePath,
    std::string displayName) {
    auto normalized =
        NormalizeExistingExecutable(executablePath);

    if (Find(normalized) != nullptr) {
        return false;
    }

    if (displayName.empty()) {
        displayName =
            DefaultDisplayName(normalized);
    }

    entries_.push_back(GameEntry{
        .executablePath = std::move(normalized),
        .displayName = std::move(displayName),
    });

    return true;
}

bool GameLibrary::Remove(
    const std::filesystem::path& executablePath) {
    std::error_code error;
    auto absolute =
        std::filesystem::absolute(
            executablePath,
            error);

    if (error) {
        return false;
    }

    absolute = absolute.lexically_normal();
    const auto key = PathKey(absolute);

    const auto iterator =
        std::find_if(
            entries_.begin(),
            entries_.end(),
            [&](const GameEntry& entry) {
                return PathKey(
                    entry.executablePath) == key;
            });

    if (iterator == entries_.end()) {
        return false;
    }

    entries_.erase(iterator);
    return true;
}

const GameEntry* GameLibrary::Find(
    const std::filesystem::path& executablePath) const noexcept {
    try {
        std::error_code error;
        auto absolute =
            std::filesystem::absolute(
                executablePath,
                error);

        if (error) {
            return nullptr;
        }

        absolute = absolute.lexically_normal();
        const auto key = PathKey(absolute);

        const auto iterator =
            std::find_if(
                entries_.begin(),
                entries_.end(),
                [&](const GameEntry& entry) {
                    return PathKey(
                        entry.executablePath) == key;
                });

        return iterator == entries_.end()
            ? nullptr
            : &*iterator;
    } catch (...) {
        return nullptr;
    }
}

const std::vector<GameEntry>&
GameLibrary::Entries() const noexcept {
    return entries_;
}

std::size_t GameLibrary::Size() const noexcept {
    return entries_.size();
}

bool GameLibrary::Empty() const noexcept {
    return entries_.empty();
}

void GameLibrary::Clear() noexcept {
    entries_.clear();
}

void GameLibrary::Save(
    const std::filesystem::path& manifestPath) const {
    const auto parent =
        manifestPath.parent_path();

    if (!parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(
            parent,
            error);

        if (error) {
            throw std::runtime_error(
                "Failed to create game library directory");
        }
    }

    std::ofstream file(
        manifestPath,
        std::ios::binary | std::ios::trunc);

    if (!file) {
        throw std::runtime_error(
            "Failed to open game library manifest");
    }

    file << kManifestHeader << '\n';

    for (const auto& entry : entries_) {
        file
            << Escape(PathKey(entry.executablePath))
            << '\t'
            << Escape(entry.displayName)
            << '\n';
    }

    if (!file) {
        throw std::runtime_error(
            "Failed to write game library manifest");
    }
}

GameLibrary GameLibrary::Load(
    const std::filesystem::path& manifestPath) {
    std::ifstream file(
        manifestPath,
        std::ios::binary);

    if (!file) {
        throw std::runtime_error(
            "Failed to open game library manifest");
    }

    std::string line;

    if (!std::getline(file, line) ||
        line != kManifestHeader) {
        throw std::runtime_error(
            "Unsupported game library manifest");
    }

    GameLibrary library;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        const auto separator =
            FindUnescapedTab(line);

        if (separator == std::string::npos) {
            throw std::runtime_error(
                "Malformed game library entry");
        }

        const auto pathText =
            Unescape(
                std::string_view(line).substr(
                    0,
                    separator));
        const auto displayName =
            Unescape(
                std::string_view(line).substr(
                    separator + 1));

        if (!library.Add(
                std::filesystem::path(pathText),
                displayName)) {
            throw std::runtime_error(
                "Game library manifest contains duplicate entries");
        }
    }

    if (!file.eof()) {
        throw std::runtime_error(
            "Failed while reading game library manifest");
    }

    return library;
}

std::filesystem::path
GameLibrary::NormalizeExistingExecutable(
    const std::filesystem::path& executablePath) {
    if (executablePath.empty()) {
        throw std::invalid_argument(
            "Game executable path cannot be empty");
    }

    std::error_code error;

    if (!std::filesystem::exists(
            executablePath,
            error) ||
        error) {
        throw std::invalid_argument(
            "Game executable does not exist");
    }

    if (!std::filesystem::is_regular_file(
            executablePath,
            error) ||
        error) {
        throw std::invalid_argument(
            "Game executable path is not a regular file");
    }

    auto absolute =
        std::filesystem::absolute(
            executablePath,
            error);

    if (error) {
        throw std::runtime_error(
            "Failed to normalize game executable path");
    }

    return absolute.lexically_normal();
}

std::string GameLibrary::DefaultDisplayName(
    const std::filesystem::path& executablePath) {
    auto name =
        executablePath.stem().string();

    if (name.empty()) {
        name =
            executablePath.filename().string();
    }

    if (name.empty()) {
        return "Untitled Game";
    }

    return name;
}

} // namespace ps5emu::library
