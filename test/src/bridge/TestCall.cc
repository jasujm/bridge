#include "bridge/BridgeConstants.hh"
#include "bridge/Call.hh"
#include "bridge/CallIterator.hh"
#include "Utility.hh"

#include <gtest/gtest.h>

TEST(CallTest, testCallIndex)
{
    for (const auto n : Bridge::to(Bridge::N_CALLS)) {
        EXPECT_EQ(n, callIndex(Bridge::enumerateCall(n)));
    }
}
