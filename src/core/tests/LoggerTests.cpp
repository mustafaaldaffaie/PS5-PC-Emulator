#include <ps5emu/logging/Logger.hpp>

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

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
    using ps5emu::logging::Level;
    using ps5emu::logging::Logger;
    using ps5emu::logging::Record;

    std::vector<Record> records;

    Logger logger(
        [&](const Record& record) {
            records.push_back(record);
        });

    logger.SetMinimumLevel(Level::Debug);

    logger.Write(
        Level::Trace,
        "test",
        "hidden");

    assert(records.empty());

    logger.Write(
        Level::Debug,
        "core",
        "first");

    logger.Write(
        Level::Error,
        "runtime",
        "second");

    assert(records.size() == 2);
    assert(records[0].sequence == 0);
    assert(records[0].level == Level::Debug);
    assert(records[0].category == "core");
    assert(records[0].message == "first");

    assert(records[1].sequence == 1);
    assert(records[1].level == Level::Error);
    assert(records[1].category == "runtime");
    assert(records[1].message == "second");

    assert(
        ps5emu::logging::LevelName(
            Level::Warning) == "warning");

    logger.SetSink(
        [&](const Record& record) {
            records.push_back(record);
        });

    logger.Write(
        Level::Critical,
        "core",
        "third");

    assert(records.size() == 3);
    assert(records[2].sequence == 2);

    assert(ThrowsInvalidArgument([] {
        Logger loggerWithNoSink(
            ps5emu::logging::Sink{});
        static_cast<void>(loggerWithNoSink);
    }));

    assert(ThrowsInvalidArgument([&] {
        logger.SetSink(
            ps5emu::logging::Sink{});
    }));

    return 0;
}
