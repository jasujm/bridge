/** \file
 *
 * \brief Definition of Bridge::Player interface
 */

#ifndef PLAYER_HH_
#define PLAYER_HH_

#include <boost/uuid/uuid.hpp>

namespace Bridge {

/** \brief Alias for boost::uuids::uuid
 *
 * Boost UUID is the preferred UUID implementation in the Bridge project
 */
using Uuid = boost::uuids::uuid;

/** \brief A bridge player
 */
class Player {
public:

    virtual ~Player() = 0;

    /** \brief Get the identity of the player
     *
     * \return UUID identifying the player
     */
    Uuid getUuid() const;

private:

    /** \brief Handle for returning UUID identifying the player
     *
     * \sa getUuid()
     */
    virtual Uuid handleGetUuid() const = 0;
};

}

#endif // PLAYER_HH_
