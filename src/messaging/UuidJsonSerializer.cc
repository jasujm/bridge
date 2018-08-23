#include "messaging/UuidJsonSerializer.hh"

#include "messaging/SerializationFailureException.hh"

#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>

namespace nlohmann {

void adl_serializer<Bridge::Uuid>::to_json(json& j, const Bridge::Uuid& uuid)
{
    j = to_string(uuid);
}

void adl_serializer<Bridge::Uuid>::from_json(const json& j, Bridge::Uuid& uuid)
{
    auto gen = boost::uuids::string_generator {};
    const auto& s = j.get_ref<const std::string&>();
    try {
        uuid = gen(s);
    } catch (const std::exception&) {
        // ANY exception at this point is assumed to have happened because of
        // invalid UUID format. Boost UUID library does not document which type
        // of exception it uses.
        throw Bridge::Messaging::SerializationFailureException {};
    }

}

}
