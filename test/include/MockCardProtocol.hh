#ifndef MOCKHAND_HH_
#define MOCKHAND_HH_

#include <gmock/gmock.h>

namespace Bridge {
namespace Main {

class MockCardProtocol : public CardProtocol {
public:
    MOCK_METHOD0(handleGetMessageHandlers, MessageHandlerRange());
    MOCK_METHOD0(handleGetCardManager, std::shared_ptr<Engine::CardManager>());
};

}
}

#endif // MOCKHAND_HH_
