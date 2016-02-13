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

#include "messaging/JsonSerializer.hh"

#include <string>

namespace Bridge {

class Contract;
enum class Doubling;

namespace Messaging {

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

/** \brief JSON converter for Doubling
 *
 * \sa JsonSerializer.hh, \ref jsoncontract
 */
template<>
struct JsonConverter<Doubling>
{
    /** \brief Convert Doubling to JSON
     */
    static nlohmann::json convertToJson(Doubling doubling);

    /** \brief Convert JSON to Doubling
     */
    static Doubling convertFromJson(const nlohmann::json& j);
};

/** \brief JSON converter for Contract
 *
 * \sa JsonSerializer.hh, \ref jsoncontract
 */
template<>
struct JsonConverter<Contract>
{
    /** \brief Convert Contract to JSON
     */
    static nlohmann::json convertToJson(const Contract& doubling);

    /** \brief Convert JSON to Contract
     */
    static Contract convertFromJson(const nlohmann::json& j);
};

}
}

#endif // MESSAGING_CONTRACTJSONSERIALIZER_HH_
