#include "messaging/EndpointIterator.hh"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

using Bridge::Messaging::EndpointIterator;

namespace {
using namespace std::string_literals;
const auto ADDRESS = "127.0.0.1"s;
const auto PORT = 5555;
}

class EndpointIteratorTest : public testing::Test {
protected:
    EndpointIterator iterator {ADDRESS, PORT};
};

TEST_F(EndpointIteratorTest, testDereference)
{
    EXPECT_EQ("tcp://127.0.0.1:5555", *iterator);
}

TEST_F(EndpointIteratorTest, testEquality)
{
    const auto other = EndpointIterator {"tcp://127.0.0.1:5555"};
    EXPECT_EQ(other, iterator);
}

TEST_F(EndpointIteratorTest, testIncrement)
{
    ++iterator;
    EXPECT_EQ("tcp://127.0.0.1:5556", *iterator);
}

TEST_F(EndpointIteratorTest, testDecrement)
{
    --iterator;
    EXPECT_EQ("tcp://127.0.0.1:5554", *iterator);
}

TEST_F(EndpointIteratorTest, testAdvance)
{
    EXPECT_EQ("tcp://127.0.0.1:5557", *(iterator + 2));
}

TEST_F(EndpointIteratorTest, testDistance)
{
    const auto other = EndpointIterator {"tcp://127.0.0.1:5557"};
    EXPECT_EQ(2, other - iterator);
}

TEST_F(EndpointIteratorTest, testInvalidProtocol)
{
    EXPECT_THROW(
        EndpointIterator {"foo://127.0.0.1:5555"}, std::invalid_argument);
}

TEST_F(EndpointIteratorTest, testInvalidPort)
{
    EXPECT_THROW(
        EndpointIterator {"tcp://127.0.0.1:abc"}, std::invalid_argument);
}
