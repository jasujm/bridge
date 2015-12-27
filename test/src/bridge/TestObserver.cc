#include "MockObserver.hh"

#include <gtest/gtest.h>

class ObserverTest : public testing::Test, protected Bridge::Observable<int> {
protected:
    virtual void SetUp()
    {
        subscribe(observer);
    }

    std::shared_ptr<Bridge::MockObserver<int>> observer {
        std::make_shared<Bridge::MockObserver<int>>()};
    std::shared_ptr<Bridge::MockObserver<int>> observer2 {
        std::make_shared<Bridge::MockObserver<int>>()};
};

TEST_F(ObserverTest, testNotify)
{
    EXPECT_CALL(*observer, handleNotify(1));
    observer->notify(1);
}

TEST_F(ObserverTest, testNotifyAllSingle)
{
    EXPECT_CALL(*observer, handleNotify(1));
    notifyAll(1);
}

TEST_F(ObserverTest, testNotifyAllMultiple)
{
    EXPECT_CALL(*observer, handleNotify(1));
    EXPECT_CALL(*observer2, handleNotify(1));

    subscribe(observer2);
    notifyAll(1);
}

TEST_F(ObserverTest, testUnsubscribe)
{
    EXPECT_CALL(*observer2, handleNotify(1));

    subscribe(observer2);
    observer.reset();
    notifyAll(1);
}
