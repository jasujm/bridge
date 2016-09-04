#ifndef MOCKCARDPROTOCOL_HH_
#define MOCKCARDPROTOCOL_HH_

#include <gmock/gmock.h>

namespace Bridge {
namespace Main {

class MockCardProtocol : public CardProtocol {
public:
    MOCK_METHOD1(handleSetAcceptor, void(std::weak_ptr<PeerAcceptor>));
    MOCK_METHOD0(handleGetMessageHandlers, MessageHandlerRange());
    MOCK_METHOD0(handleGetSockets, SocketRange());
    MOCK_METHOD0(handleGetCardManager, std::shared_ptr<Engine::CardManager>());
};

class MockPeerAcceptor : public CardProtocol::PeerAcceptor {
public:
    MOCK_METHOD2(
        acceptPeer,
        CardProtocol::PeerAcceptState(
            const std::string&, const CardProtocol::PositionVector&));
};

}
}

#endif // MOCKCARDPROTOCOL_HH_
