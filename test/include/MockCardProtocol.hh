#ifndef MOCKCARDPROTOCOL_HH_
#define MOCKCARDPROTOCOL_HH_

#include <gmock/gmock.h>

namespace Bridge {
namespace Main {

class MockCardProtocol : public CardProtocol {
public:
    MOCK_METHOD2(handleAcceptPeer,
        bool(const std::string& identity, const PositionVector&));
    MOCK_METHOD0(handleInitialize, void());
    MOCK_METHOD0(handleGetMessageHandlers, MessageHandlerVector());
    MOCK_METHOD0(handleGetSockets, SocketVector());
    MOCK_METHOD0(handleGetCardManager, std::shared_ptr<Engine::CardManager>());
};

}
}

#endif // MOCKCARDPROTOCOL_HH_
