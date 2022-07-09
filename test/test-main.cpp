#include "gtest/gtest.h"
#include "configcat/consolelogger.h"

using namespace configcat;

int main(int argc, char **argv) {
    ConsoleLogger logger;
    setLogger(&logger);
    setLogLevel(LOG_LEVEL_DEBUG);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
