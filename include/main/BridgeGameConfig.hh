#ifndef MAIN_BRIDGEGAMECONFIG_HH_
#define MAIN_BRIDGEGAMECONFIG_HH_

#include "bridge/Uuid.hh"
#include "bridge/Position.hh"
#include "Blob.hh"

#include <boost/operators.hpp>

#include <memory>
#include <vector>

#include <zmq.hpp>

namespace Bridge {

namespace Messaging {

struct CurveKeys;

}

namespace Main {

class BridgeGame;
class CallbackScheduler;

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
    };

    // Explicitly instantiate to preserve aggregate-ness of PeerConfig
    struct boost::equality_comparable<PeerConfig>;

    Uuid uuid;                         ///< Game UUID
    std::vector<Position> positionsControlled; ///< Positions controlled by self
    std::vector<PeerConfig> peers;     ///< Peers taking part in the game
};

/** \brief Equality comparison between peer config objects
 */
bool operator==(
    const BridgeGameConfig::PeerConfig&, const BridgeGameConfig::PeerConfig&);

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
 */
BridgeGame gameFromConfig(
    const BridgeGameConfig& config,
    zmq::context_t& context,
    const Messaging::CurveKeys* keys,
    std::shared_ptr<zmq::socket_t> eventSocket,
    std::shared_ptr<CallbackScheduler> callbackScheduler);

}
}

#endif // MAIN_BRIDGEGAMECONFIG_HH_
