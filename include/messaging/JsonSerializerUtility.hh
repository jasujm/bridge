/** \file
 *
 * \brief Definition of JSON serialization utilities
 */

#include "messaging/JsonSerializer.hh"
#include "messaging/SerializationFailureException.hh"

#include <boost/optional/optional.hpp>
#include <json.hpp>

#include <utility>

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
 * \throw SerializationFailureException if \p j does not represent string or does
 * not represent valid enumeration.
 */
template<typename E, typename MapType>
E jsonToEnum(const nlohmann::json& j, const MapType& map)
{
    const auto iter = map.find(fromJson<std::string>(j));
    if (iter != map.end()) {
        return iter->second;
    }
    throw SerializationFailureException {};
}

/** \brief Access element in JSON object
 *
 * This function is used to return an element from JSON object and convert it
 * to the desired type. It uses fromJson() to do the conversion.
 *
 * \tparam T the type the JSON element is converted to
 *
 * \param j the json object
 * \param key the key of the element
 *
 * \return the accessed object converted to \p T
 *
 * \throw SerializationFailureException in the followin cases:
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
    throw SerializationFailureException {};
}
/** \brief Put optional value to JSON object if it exists
 *
 * If \p t is none, does nothing, otherwise converts the object to JSON and
 * adds it to \p j.
 *
 * \param j the JSON object where the value is put
 * \param key the key of the optional value
 * \param t the optional value
 */
template<typename T>
void optionalPut(
    nlohmann::json& j, const nlohmann::json::object_t::key_type& key,
    const boost::optional<T>& t)
{
    if (t) {
        j[key] = toJson(*t);
    }
}

/** \brief Access optional element in JSON object
 *
 * This function is used to return an optional element from JSON object and
 * convert it to the desired type.
 *
 * \tparam T the type the JSON element is converted to
 *
 * \param j the json object
 * \param key the key of the element
 *
 * \return The accessed object converted to \p T, or none if \p key does’t
 * exist in the object
 */
template<typename T>
boost::optional<T> optionalGet(
    const nlohmann::json& j, const nlohmann::json::object_t::key_type& key)
{
    const auto iter = j.find(key);
    if (iter != j.end()) {
        return fromJson<T>(*iter);
    }
    return boost::none;
}

/** \brief Convert pair to JSON object
 *
 * The returned JSON object contains two key‐value pairs with \p key1 and \p
 * key2 mapping to each member of the pair converted to JSON.
 *
 * \param p the pair to convert
 * \param key1 the key for the first member in \p p
 * \param key2 the key for the second member in \p p
 *
 * \return JSON representation of the pair
 *
 * \sa jsonToPair()
 */
template<typename T1, typename T2>
nlohmann::json pairToJson(
    const std::pair<T1, T2>& p, const nlohmann::json::object_t::key_type& key1,
    const nlohmann::json::object_t::key_type& key2)
{
    return {
        { key1, toJson(p.first) },
        { key2, toJson(p.second) }
    };
}

/** \brief Convert JSON object to pair
 *
 * The function excepts a JSON object with two key‐value pair. Keys \p key1
 * and \p key2 are converted to the first and second element of the returned
 * pair, respectively.
 *
 * \param j the JSON object to convert
 * \param key1 the key of the first member of the pair
 * \param key2 the key of the first member of the pair
 *
 * \return The pair constructed from \p j
 *
 * \sa jsonToPair()
 */
template<typename T1, typename T2>
std::pair<T1, T2> jsonToPair(
    const nlohmann::json& j, const nlohmann::json::object_t::key_type& key1,
    const nlohmann::json::object_t::key_type& key2)
{
    return {
        checkedGet<T1>(j, key1),
        checkedGet<T2>(j, key2)
    };
}

/** \brief JSON converter for optional types
 *
 * \sa JsonSerializer.hh
 */
template<typename T>
struct JsonConverter<boost::optional<T>>
{
    /** \brief Convert optional type to JSON
     *
     * \param t the optional value to convert
     *
     * \return If \p t contains value, applies toJson() to the contained value
     * and returns the result. Otherwise returns \c null.
     */
    static nlohmann::json convertToJson(const boost::optional<T>& t);

    /** \brief Convert JSON to optional type
     *
     * \tparam T the type contained by the optional type
     *
     * \param j the JSON object to convert
     *
     * \return If \p j is not \c null, applies fromJson() to it and returns the
     * result in optional value. Otherwise returns \c none.
     *
     * \sa optionalToJson()
     */
    static boost::optional<T> convertFromJson(const nlohmann::json& j);
};

template<typename T>
nlohmann::json JsonConverter<boost::optional<T>>::convertToJson(
    const boost::optional<T>& t)
{
    if (t) {
        return toJson(*t);
    }
    return nullptr;
}

template<typename T>
boost::optional<T> JsonConverter<boost::optional<T>>::convertFromJson(
    const nlohmann::json& j)
{
    if (j.is_null()) {
        return boost::none;
    }
    return fromJson<T>(j);
}

/** \brief JSON converter for vectors
 *
 * \sa JsonSerializer.hh
 */
template<typename T>
struct JsonConverter<std::vector<T>>
{

    /** \brief Convert vector to JSON
     *
     * \param v the vector to convert
     *
     * \return JSON array consisting of each element of \c v converted to JSON
     * using toJson().
     */
    static nlohmann::json convertToJson(const std::vector<T>& v);

    /** \brief Convert JSON to vector
     *
     * \param j the JSON array to convert
     *
     * \return Vector consisting of each element of \j converted from JSON
     * using fromJson().
     */
    static std::vector<T> convertFromJson(const nlohmann::json& j);
};

template<typename T>
nlohmann::json JsonConverter<std::vector<T>>::convertToJson(
    const std::vector<T>& v)
{
    auto ret = nlohmann::json::array();
    for (const auto& t : v) {
        ret.push_back(toJson(t));
    }
    return ret;
}

template<typename T>
std::vector<T> JsonConverter<std::vector<T>>::convertFromJson(
    const nlohmann::json& j)
{
    auto ret = std::vector<T> {};
    for (const auto& t : j) {
        ret.push_back(fromJson<T>(t));
    }
    return ret;
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
 * throws SerializationFailureException.
 *
 * \tparam Pred, Preds Predicates that can be invoked with the object to
 * validate and whose return value is contextually convertible to bool.
 *
 * \param t the object to validate
 * \param pred, rest the predicates used to validate \p t
 *
 * \return \p t if all predicates evaluate to true
 *
 * \throw SerializationFailureException if any predicate evaluates to false
 */
template<typename T, typename Pred, typename... Preds>
decltype(auto) validate(T&& t, Pred&& pred, Preds&&... rest)
{
    if (pred(t)) {
        return validate(t, rest...);
    }
    throw SerializationFailureException {};
}
/// \}

}
}

#endif // MESSAGING_JSONSERIALIZERUTILITY_HH_