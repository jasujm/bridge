#ifndef MOCKCARDPROTOCOL_HH_
#define MOCKCARDPROTOCOL_HH_

#include <gmock/gmock.h>

namespace Bridge {
namespace Main {

class MockCardProtocol : public CardProtocol {
public:
    MOCK_METHOD3(
        handleAcceptPeer,
        bool(
            const Messaging::Identity& identity, const PositionVector&,
            const std::optional<nlohmann::json>&));
    MOCK_METHOD0(handleInitialize, void());
    MOCK_METHOD0(handleGetMessageHandlers, MessageHandlerVector());
    MOCK_METHOD0(handleGetSockets, SocketVector());
    MOCK_METHOD0(handleGetCardManager, std::shared_ptr<Engine::CardManager>());
};

}
}

#endif // MOCKCARDPROTOCOL_HH_
