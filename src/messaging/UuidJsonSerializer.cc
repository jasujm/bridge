#include "messaging/UuidJsonSerializer.hh"

#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>

using nlohmann::json;

namespace Bridge {
namespace Messaging {

json JsonConverter<Uuid>::convertToJson(const Uuid& uuid)
{
    return to_string(uuid);
}

Uuid JsonConverter<Uuid>::convertFromJson(const json& j)
{
    boost::uuids::string_generator gen;
    const auto s = j.get<std::string>();
    try {
        return gen(s);
    } catch (const std::exception&) {
        // ANY exception at this point is assumed to have happened because of
        // invalid UUID format. Boost UUID library does not document which type
        // of exception it uses.
        throw SerializationFailureException {};
    }

}

}
}
