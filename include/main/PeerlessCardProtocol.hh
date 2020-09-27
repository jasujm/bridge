/** \file
 *
 * \brief Definition of Bridge::Main::PeerlessCardProtocol
 */

#ifndef MAIN_PEERSLESSCARDPROTOCOL_HH_
#define MAIN_PEERSLESSCARDPROTOCOL_HH_

#include "engine/SimpleCardManager.hh"
#include "main/CardProtocol.hh"
#include "FunctionObserver.hh"

#include <boost/core/noncopyable.hpp>

#include <memory>

namespace Bridge {
namespace Main {

/** \brief Peerless card protocol
 *
 * PeerlessCardProtocol implements a very simple “protocol” for games without
 * peers. It simply immediately generates a randomly shuffled deck when
 * requested. It accepts no peers.
 */
class PeerlessCardProtocol : public CardProtocol, private boost::noncopyable {
public:

    /** \brief Create new peerless card protocol
     *
     * \param args The arguments passed to the underlying SimpleCardManager
     * constructor
     */
    template<typename... Args>
    PeerlessCardProtocol(Args&&... args);

private:

    static void internalShuffle(
        Engine::SimpleCardManager& cardManager,
        Engine::CardManager::ShufflingState state);

    bool handleAcceptPeer(
        const Messaging::Identity& identity, const PositionVector& positions,
        const OptionalArgs& args) override;

    void handleInitialize() override;

    std::shared_ptr<Messaging::MessageHandler>
    handleGetDealMessageHandler() override;

    SocketVector handleGetSockets() override;

    std::shared_ptr<Engine::CardManager> handleGetCardManager() override;

    std::shared_ptr<Engine::SimpleCardManager> cardManager;
    std::shared_ptr<Observer<Engine::CardManager::ShufflingState>> shuffler;
};

template<typename... Args>
PeerlessCardProtocol::PeerlessCardProtocol(Args&&... args) :
    cardManager {
        std::make_shared<Engine::SimpleCardManager>(
            std::forward<Args>(args)...)},
    shuffler {
        makeObserver<Engine::CardManager::ShufflingState>(
            [this](const auto& state)
            {
                assert(cardManager);
                internalShuffle(*cardManager, state);
            })}
{
    cardManager->subscribe(shuffler);
}

}
}

#endif // MAIN_PEERSLESSCARDPROTOCOL_HH_
