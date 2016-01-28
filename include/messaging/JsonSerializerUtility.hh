/** \file
 *
 * \brief Definition of JSON serialization utilities
 */

#include "messaging/JsonSerializer.hh"
#include "messaging/MessageHandlingException.hh"

#include <json.hpp>

#ifndef MESSAGING_JSONSERIALIZERUTILITY_HH_
#define MESSAGING_JSONSERIALIZERUTILITY_HH_

namespace Bridge {
namespace Messaging {

/** \brief Convert enumeration to JSON
 *
 * \param e the enumeration to convert
 * \param map mapping between enumeration values and strings
 *
 * \return JSON representation of \p e
 */
template<typename E, typename MapType>
nlohmann::json enumToJson(E e, const MapType& map)
{
    return toJson<std::string>(map.at(e));
}

/** \brief Convert JSON object to enumeration
 *
 * \tparam E the enumeration type
 *
 * \param j the JSON object to convert
 * \param map mapping between strings and enumeration values
 *
 * \return the enumeration represented by \p j
 *
 * \throw MessageHandlingException if \p j does not represent string or does
 * not represent valid enumeration.
 */
template<typename E, typename MapType>
E jsonToEnum(const nlohmann::json& j, const MapType& map)
{
    const auto iter = map.find(fromJson<std::string>(j));
    if (iter != map.end()) {
        return iter->second;
    }
    throw MessageHandlingException {};
}

/** \brief Access elements in JSON object
 *
 * This function is used to return an element from JSON object and convert it
 * to the desired type. It uses fromJson() to do the conversion.
 *
 * \tparam T the type the JSON element is converted to
 *
 * \param j the json object
 * \param key the key of the element
 *
 * \throw MessageHandlingException in the followin cases:
 *   - \p j does not represent object or does not have key \p key
 *   - Converting \p j to type \p T fails
 */
template<typename T>
auto checkedGet(
    const nlohmann::json& j, const nlohmann::json::object_t::key_type& key)
{
    const auto iter = j.find(key);
    if (iter != j.end()) {
        return fromJson<T>(*iter);
    }
    throw MessageHandlingException {};
}

/// \{
template<typename T>
decltype(auto) validate(T&& t)
{
    return t;
}

/** \brief Validate a value
 *
 * This function is intended to be used for an object that was deserialized
 * from JSON to perform additional validation. The client passes value to
 * validate and at least one predicate to perform the validation. The function
 * returns the value if all the predicates evaluate to true and otherwise
 * throws MessageHandlingException.
 *
 * \tparam Pred, Preds Predicates that can be invoked with the object to
 * validate and whose return value is contextually convertible to bool.
 *
 * \param t the object to validate
 * \param pred, rest the predicates used to validate \p t
 *
 * \return \p t if all predicates evaluate to true
 *
 * \throw MessageHandlingException if any predicate evaluates to false
 */
template<typename T, typename Pred, typename... Preds>
decltype(auto) validate(T&& t, Pred&& pred, Preds&&... rest)
{
    if (pred(t)) {
        return validate(t, rest...);
    }
    throw MessageHandlingException {};
}
/// \}

}
}

#endif // MESSAGING_JSONSERIALIZERUTILITY_HH_
