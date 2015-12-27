/** \file
 *
 * \brief Definition of Bridge::Engine::SimpleCardManager class
 */

#ifndef ENGINE_SIMPLECARDMANAGER_HH_
#define ENGINE_SIMPLECARDMANAGER_HH_

#include "bridge/SimpleCard.hh"
#include "engine/CardManager.hh"

#include <boost/core/noncopyable.hpp>

namespace Bridge {
namespace Engine {

/** \brief A simple card manager for local play
 *
 * SimpleCardManager just generates a randomly shuffled 52 card playing card
 * deck locally.
 */
class SimpleCardManager : public CardManager, private boost::noncopyable {
private:

    void handleRequestShuffle() override;

    std::unique_ptr<Hand> handleGetHand(
        const std::vector<std::size_t>& ns) override;

    std::size_t handleGetNumberOfCards() const override;

    std::vector<SimpleCard> cards;
};

}
}

#endif // ENGINE_SIMPLECARDMANAGER_HH_
