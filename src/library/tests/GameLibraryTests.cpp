#include <ps5emu/library/GameLibrary.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

template <typename Exception, typename Function>
bool Throws(Function&& function) {
    try {
        function();
        return false;
    } catch (const Exception&) {
        return true;
    }
}

void CreateFile(
    const std::filesystem::path& path,
    std::string_view contents = "ELF") {
    std::ofstream file(
        path,
        std::ios::binary | std::ios::trunc);

    assert(file.good());
    file << contents;
    assert(file.good());
}

} // namespace

int main() {
    using ps5emu::library::GameLibrary;

    const auto root =
        std::filesystem::temp_directory_path() /
        "ps5emu-game-library-tests";

    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root);

    const auto first = root / "game-one.elf";
    const auto second = root / "eboot.bin";

    CreateFile(first);
    CreateFile(second);

    GameLibrary library;

    assert(library.Empty());
    assert(library.Add(first));
    assert(library.Size() == 1);
    assert(!library.Add(first));
    assert(library.Size() == 1);

    const auto* firstEntry =
        library.Find(first);

    assert(firstEntry != nullptr);
    assert(firstEntry->displayName == "game-one");
    assert(
        firstEntry->executablePath.is_absolute());

    assert(
        library.Add(
            second,
            "Second\tGame\nName"));

    assert(library.Size() == 2);

    const auto manifest =
        root / "state" / "library.txt";

    library.Save(manifest);

    const auto loaded =
        GameLibrary::Load(manifest);

    assert(loaded.Size() == 2);

    const auto* loadedFirst =
        loaded.Find(first);
    const auto* loadedSecond =
        loaded.Find(second);

    assert(loadedFirst != nullptr);
    assert(
        loadedFirst->displayName ==
        "game-one");

    assert(loadedSecond != nullptr);
    assert(
        loadedSecond->displayName ==
        "Second\tGame\nName");

    GameLibrary mutableCopy = loaded;

    assert(mutableCopy.Remove(first));
    assert(!mutableCopy.Remove(first));
    assert(mutableCopy.Size() == 1);

    mutableCopy.Clear();
    assert(mutableCopy.Empty());

    assert(Throws<std::invalid_argument>([&] {
        static_cast<void>(
            library.Add(root / "missing.elf"));
    }));

    assert(Throws<std::invalid_argument>([&] {
        static_cast<void>(
            library.Add(root));
    }));

    {
        const auto malformed =
            root / "bad.txt";

        std::ofstream file(malformed);
        file
            << "PS5EMU_GAME_LIBRARY_V1\n"
            << "missing-tab\n";
        file.close();

        assert(Throws<std::runtime_error>([&] {
            static_cast<void>(
                GameLibrary::Load(malformed));
        }));
    }

    {
        const auto unsupported =
            root / "old.txt";

        std::ofstream file(unsupported);
        file << "PS5EMU_GAME_LIBRARY_V0\n";
        file.close();

        assert(Throws<std::runtime_error>([&] {
            static_cast<void>(
                GameLibrary::Load(unsupported));
        }));
    }

    std::filesystem::remove_all(root, error);
    return 0;
}
