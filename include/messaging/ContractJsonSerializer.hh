/** \file
 *
 * \brief Definition of JSON serializer for Bridge::Contract
 *
 * \page jsoncontract Contract JSON representation
 *
 * A Bridge::Contract is represented by a JSON object consisting of the
 * following:
 *
 * \code{.json}
 * {
 *     { "bid": <bid> },
 *     { "doubling": <doubling> }
 * }
 * \endcode
 *
 * - &lt;level&gt; a bid, see \ref jsonbid
 * - &lt;strain&gt; is a string representing the doubling state of the
 *   contract. It must be one of the following: "undoubled", "doubled",
 *   "redoubled".
 */

#ifndef MESSAGING_CONTRACTJSONSERIALIZER_HH_
#define MESSAGING_CONTRACTJSONSERIALIZER_HH_

#include <nlohmann/json.hpp>

#include <string>

namespace Bridge {

class Contract;
struct Doubling;

/** \brief Key for Contract::bid in JSON object
 *
 * \sa \ref jsoncontract
 */
extern const std::string CONTRACT_BID_KEY;

/** \brief Key for Contract::doubling in JSON object
 *
 * \sa \ref jsoncontract
 */
extern const std::string CONTRACT_DOUBLING_KEY;

/** \brief Convert Contract to JSON
 */
void to_json(nlohmann::json&, const Contract&);

/** \brief Convert JSON to Contract
 */
void from_json(const nlohmann::json&, Contract&);

}

#endif // MESSAGING_CONTRACTJSONSERIALIZER_HH_
