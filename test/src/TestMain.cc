#include <iostream>

#include <gtest/gtest.h>

#include "Logging.hh"

int main(int argc, char* argv[])
{
    Bridge::setupLogging(Bridge::LogLevel::NONE, std::cerr);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
