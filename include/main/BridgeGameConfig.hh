/** \file
 *
 * \brief Definition of Bridge::Main::BridgeGameConfig class
 */


#ifndef MAIN_BRIDGEGAMECONFIG_HH_
#define MAIN_BRIDGEGAMECONFIG_HH_

#include "bridge/Uuid.hh"
#include "bridge/Position.hh"
#include "messaging/Authenticator.hh"
#include "messaging/Sockets.hh"
#include "Blob.hh"

#include <memory>
#include <optional>
#include <vector>

namespace Bridge {

namespace Messaging {

class CallbackScheduler;
struct CurveKeys;

}

namespace Main {

class BridgeGame;

/** \brief Bridge game configuration info
 *
 * An object containing the necessary configuration data needed to set up a
 * bridge game. The intention is that BridgeGameConfig is used as an
 * intermediate representation of the game coming from config file or other
 * source, and that will be used as a template to create a functioning
 * BridgeGame object.
 */
struct BridgeGameConfig {

    /** \brief Bridge game peer config
     */
    struct PeerConfig {
        std::string endpoint;          ///< Peer endpoint
        Blob serverKey;                ///< Peer server key

        /// \brief Equality operator
        bool operator==(const PeerConfig&) const = default;
    };

    /** \brief Bridge game card server config
     */
    struct CardServerConfig {
        std::string controlEndpoint;   ///< Card server control endpoint
        std::string peerEndpoint;      ///< Card server peer endpoint
        Blob serverKey;                ///< Server key for the card server

        /// \brief Equality operator
        bool operator==(const CardServerConfig&) const = default;
    };

    Uuid uuid;                                 ///< Game UUID
    bool isDefault;                            ///< Game is default
    std::vector<Position> positionsControlled; ///< Positions controlled by self
    std::vector<PeerConfig> peers;     ///< Peers taking part in the game
    std::optional<CardServerConfig> cardServer; ///< Card server config
};

/** \brief Construct bridge game object from config
 *
 * This function uses \p config as input to construct and initialize a
 * BridgeGame. Initializing means that it connects to peer endpoints and sends
 * them the necessary handshake messages to set up the game and card
 * exchange. Thus the object returned is ready to be used to handle commands
 * from clients and peers.
 *
 * \param config the game configurations
 * \param context the ZeroMQ context
 * \param keys the CurveZMQ keys for the peer
 * \param eventSocket the event socket passed to BridgeGame constructor
 * \param callbackScheduler the callback scheduler passed to BridgeGame
 *        constructor
 * \param knownNodes the mapping of known peer public keys and their identities
 */
BridgeGame gameFromConfig(
    const BridgeGameConfig& config,
    Messaging::MessageContext& context,
    const Messaging::CurveKeys* keys,
    Messaging::SharedSocket eventSocket,
    std::shared_ptr<Messaging::CallbackScheduler> callbackScheduler,
    const Messaging::Authenticator::NodeMap& knownNodes);

}
}

#endif // MAIN_BRIDGEGAMECONFIG_HH_
