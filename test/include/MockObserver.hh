#ifndef MOCKOBSERVER_HH_
#define MOCKOBSERVER_HH_

#include "Observer.hh"

#include <gmock/gmock.h>

namespace Bridge {

template<typename T>
class MockObserver : public Observer<T>
{
public:
    MOCK_METHOD1_T(handleNotify, void(const T&));
};

}

#endif // MOCKOBSERVER_HH_
