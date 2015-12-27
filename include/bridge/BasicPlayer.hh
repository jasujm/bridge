/** \file
 *
 * \brief Definition of Bridge::BasicPlayer class
 */

#ifndef BASICPLAYER_HH_
#define BASICPLAYER_HH_

#include "bridge/Player.hh"

#include <boost/core/noncopyable.hpp>

namespace Bridge {

/** \brief Concrete implementation of the Player interface
 */
class BasicPlayer : public Player, private boost::noncopyable {
};

}

#endif // BASICPLAYER_HH_
