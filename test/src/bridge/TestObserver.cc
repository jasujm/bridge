#include "MockObserver.hh"

#include "FunctionObserver.hh"

#include <gtest/gtest.h>

class ObserverTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        observable.subscribe(observer);
    }

    std::shared_ptr<Bridge::MockObserver<int>> observer {
        std::make_shared<Bridge::MockObserver<int>>()};
    std::shared_ptr<Bridge::MockObserver<int>> observer2 {
        std::make_shared<Bridge::MockObserver<int>>()};
    Bridge::Observable<int> observable;
};

TEST_F(ObserverTest, testNotify)
{
    EXPECT_CALL(*observer, handleNotify(1));
    observer->notify(1);
}

TEST_F(ObserverTest, testNotifyAllSingle)
{
    EXPECT_CALL(*observer, handleNotify(1));
    observable.notifyAll(1);
}

TEST_F(ObserverTest, testNotifyAllMultiple)
{
    EXPECT_CALL(*observer, handleNotify(1));
    EXPECT_CALL(*observer2, handleNotify(1));

    observable.subscribe(observer2);
    observable.notifyAll(1);
}

TEST_F(ObserverTest, testUnsubscribe)
{
    EXPECT_CALL(*observer2, handleNotify(1));

    observable.subscribe(observer2);
    observer.reset();
    observable.notifyAll(1);
}

TEST_F(ObserverTest, testFunctionObserver)
{
    auto function_observer = Bridge::makeFunctionObserver<int>(
        [this](int t)
        {
            observer2->notify(t);
        });

    EXPECT_CALL(*observer2, handleNotify(3));
    observable.subscribe(function_observer);
    observable.notifyAll(3);
}
