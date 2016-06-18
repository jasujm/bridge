/** \file
 *
 * \brief Definition of Bridge::Main::SimpleCardProtocol
 */

#include "main/CardProtocol.hh"

#include <boost/core/noncopyable.hpp>
#include <boost/range/any_range.hpp>

#include <memory>
#include <functional>

namespace Bridge {
namespace Main {

class PeerCommandSender;

/** \brief Simple plaintext card protocol
 *
 * SimpleCardProtocol implements a simple plaintext card protocol where one
 * player generates the cards and sends them to all, unencrypted. This protocl
 * should only be used between trusted parties.
 */
class SimpleCardProtocol : public CardProtocol, private boost::noncopyable {
public:

    /** \brief Command for dealing cards to peers
     *
     * \sa \ref bridgeprotocolcontroldeal
     */
    static const std::string DEAL_COMMAND;

    /** \brief Parameter to SimpleCardProtocol()
     */
    using IsLeaderFunction = std::function<bool(const std::string*)>;

    /** \brief Create simple card manager
     *
     * \param isLeader Function that is used to determine who is the leader
     * that should generate the shuffled cards. If called with nullptr, the
     * function should return true if this application itself is the leader,
     * or false otherwise. If called with a valid pointer, the function should
     * return true if the peer with the given identity is the leader, or false
     * otherwise.
     * \param peerCommandSender A peer command sender that can be used to send
     * commands to the peers.
     */
    SimpleCardProtocol(
        IsLeaderFunction isLeader,
        std::shared_ptr<PeerCommandSender> peerCommandSender);

private:

    MessageHandlerRange handleGetMessageHandlers() override;

    std::shared_ptr<Engine::CardManager> handleGetCardManager() override;

    class Impl;
    const std::shared_ptr<Impl> impl;
};


}
}
