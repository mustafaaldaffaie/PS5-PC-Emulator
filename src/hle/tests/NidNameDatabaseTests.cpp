#include <ps5emu/hle/Nid.hpp>
#include <ps5emu/hle/NidNameDatabase.hpp>

#include <cassert>
#include <stdexcept>
#include <string>

namespace {

template <typename Function>
bool ThrowsInvalidArgument(Function&& function) {
    try {
        function();
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

} // namespace

int main() {
    auto database =
        ps5emu::hle::NidNameDatabase::CreateBuiltIn();

    assert(database.Size() == 17);

    const auto* kernelWriteName =
        database.FindName("4wSze92BhLI");
    assert(kernelWriteName != nullptr);
    assert(*kernelWriteName == "sceKernelWrite");

    const auto* kernelWriteNid =
        database.FindNid("sceKernelWrite");
    assert(kernelWriteNid != nullptr);
    assert(*kernelWriteNid == "4wSze92BhLI");

    assert(
        *kernelWriteNid ==
        ps5emu::hle::Nid::Compute("sceKernelWrite"));

    assert(database.FindName("AAAAAAAAAAA") == nullptr);
    assert(database.FindNid("missingSymbol") == nullptr);

    assert(!database.Add("sceKernelWrite"));
    assert(database.Size() == 17);

    assert(database.Add("customSymbol"));
    assert(database.Size() == 18);

    const auto customNid =
        ps5emu::hle::Nid::Compute("customSymbol");
    const auto* customName = database.FindName(customNid);
    assert(customName != nullptr);
    assert(*customName == "customSymbol");

    assert(ThrowsInvalidArgument([&] {
        static_cast<void>(database.Add(""));
    }));

    assert(ThrowsInvalidArgument([&] {
        static_cast<void>(
            database.Add(std::string("bad\0name", 8)));
    }));

    database.Clear();
    assert(database.Size() == 0);

    return 0;
}
