#include "FunctionQueue.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <exception>

namespace {
class MockFunction {
public:
    MOCK_METHOD0(call1, void());
    MOCK_METHOD0(call2, void());
    MOCK_METHOD0(call3, void());
};
}

class FunctionQueueTest : public testing::Test {
protected:
    MockFunction function;
    Bridge::FunctionQueue functionQueue;
};

TEST_F(FunctionQueueTest, testFunctionQueue)
{
    EXPECT_CALL(function, call1());
    EXPECT_CALL(function, call2());
    EXPECT_CALL(function, call3());
    functionQueue(
        [this]()
        {
            function.call1();
            functionQueue(
                [this]()
                {
                    function.call3();
                });
            function.call2();
        });
}

TEST_F(FunctionQueueTest, testException)
{
    EXPECT_CALL(function, call1());
    EXPECT_CALL(function, call2()).Times(0);
    EXPECT_THROW(
        functionQueue(
            [this]()
            {
                function.call1();
                functionQueue(
                    [this]()
                    {
                        function.call2();
                    });
                throw std::exception {};
            }),
        std::exception);

    testing::Mock::VerifyAndClearExpectations(&function);
    EXPECT_CALL(function, call1());
    functionQueue([this]() { function.call1(); });
}
