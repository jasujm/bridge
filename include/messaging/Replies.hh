/** \file
 *
 * \brief Definition of standard replies to messages
 */

#ifndef MESSAGING_REPLIES_HH_
#define MESSAGING_REPLIES_HH_

#include "Blob.hh"

namespace Bridge {
namespace Messaging {

/** \brief Generic successful status code
 *
 * \note The particular value contains characters OK if dumped as ASCII
 */
extern const ByteSpan REPLY_SUCCESS;

/** \brief Generic failed status code
 *
 * \note The particular value contains characters ERR if dumped as ASCII
 */
extern const ByteSpan REPLY_FAILURE;

/**
 * \brief Determine if a status code is successful
 *
 * \param status the status code
 *
 * \return true if \p status has prefix OK, false otherwise
 */
bool isSuccessful(ByteSpan status);

}
}

#endif // MESSAGING_REPLIES_HH_
