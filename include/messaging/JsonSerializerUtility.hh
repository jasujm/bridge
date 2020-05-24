/** \file
 *
 * \brief Definition of JSON serialization utilities
 */

#include "messaging/JsonSerializer.hh"
#include "messaging/SerializationFailureException.hh"
#include "Blob.hh"

#include "enhanced_enum/enhanced_enum.hh"

#include <nlohmann/json.hpp>

#include <functional>
#include <optional>
#include <utility>

#ifndef MESSAGING_JSONSERIALIZERUTILITY_HH_
#define MESSAGING_JSONSERIALIZERUTILITY_HH_

namespace nlohmann {

/** \brief JSON converter for optional types
 */
template<typename T>
struct adl_serializer<std::optional<T>>
{
    /** \brief Convert optional type to JSON
     */
    static void to_json(json&, const std::optional<T>&);

    /** \brief Convert JSON to optional type
     */
    static void from_json(const json&, std::optional<T>&);
};

template<typename T>
void adl_serializer<std::optional<T>>::to_json(
    json& j, const std::optional<T>& t)
{
    if (t) {
        j = *t;
    } else {
        j = nullptr;
    }
}

template<typename T>
void adl_serializer<std::optional<T>>::from_json(
    const json& j, std::optional<T>& t)
{
    if (j.is_null()) {
        t = std::nullopt;
    } else {
        t = j.get<T>();
    }
}

/** \brief JSON converter for blobs
 */
template<>
struct adl_serializer<Bridge::Blob>
{
    /** \brief Convert blob to JSON
     */
    static void to_json(json&, const Bridge::Blob&);

    /** \brief Convert JSON to blob
     */
    static void from_json(const json&, Bridge::Blob&);
};

/** \brief JSON converter for enums
 */
template<typename Enum>
struct adl_serializer<Enum, std::enable_if_t<enhanced_enum::is_enhanced_enum_v<Enum>>>
{
    /** \brief Convert optional type to JSON
     */
    static void to_json(json& j, const Enum& e)
    {
        j = e.value();
    }

    /** \brief Convert JSON to optional type
     */
    static void from_json(const json& j, Enum& e)
    {
        if (const auto opt_e = Enum::from(j.get<typename Enum::value_type>())) {
            e = *opt_e;
        } else {
            throw Bridge::Messaging::SerializationFailureException {};
        }
    }
};

}

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
    return map.at(e);
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
    const auto iter = map.find(j);
    if (iter != map.end()) {
        return iter->second;
    }
    throw SerializationFailureException {};
}

/** \brief Convert blob to JSON
 *
 * \param blob the blob to convert
 *
 * \return JSON string containing the hex encoded blob
 */
nlohmann::json blobToJson(const Blob& blob);

/** \brief Convert JSON to blob
 *
 * \param j JSON string containing the hex encoded blob
 *
 * \return the decoded blob
 *
 * \throw SerializationFailureException if \p j does not contain valid hex
 * encoded string
 */
Blob jsonToBlob(const nlohmann::json& j);

/** \brief Validate a deserialized value
 *
 * This function is intended to be used for an deserialized object when
 * additional validation is needed.
 *
 * \tparam Preds Predicates that can be invoked with \p t and whose return value
 * is convertible to bool.
 *
 * \param t the object to validate
 * \param preds the predicates used to validate \p t
 *
 * \return the object \p t if all predicates evaluate to true
 *
 * \throw SerializationFailureException if any predicate evaluates to false
 */
template<typename T, typename... Preds>
auto validate(T&& t, Preds&&... preds)
{
    if ( ( ... && std::invoke(std::forward<Preds>(preds), t) ) ) {
        return std::forward<T>(t);
    }
    throw SerializationFailureException {};
}

/** \brief Convert JSON to object, ignoring errors
 *
 * This function tries to convert JSON object to an object of type \c T, except
 * it catches any exceptions and returns empty value instead on error.
 *
 * \param j the JSON object to covert
 *
 * \return \p j converted to object of type \c T, or none if exception is thrown
 * while converting
 */
template<typename T>
std::optional<T> tryFromJson(const nlohmann::json& j)
{
    try {
        return j.get<T>();
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}
}

#endif // MESSAGING_JSONSERIALIZERUTILITY_HH_
