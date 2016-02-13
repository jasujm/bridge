#include "messaging/CallJsonSerializer.hh"

#include "messaging/BidJsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/MessageHandlingException.hh"

#include <functional>
#include <map>

using nlohmann::json;

namespace Bridge {
namespace Messaging {

const std::string CALL_TYPE_KEY {"type"};
const std::string CALL_PASS_TAG {"pass"};
const std::string CALL_BID_TAG {"bid"};
const std::string CALL_DOUBLE_TAG {"double"};
const std::string CALL_REDOUBLE_TAG {"redouble"};

namespace {

class JsonSerializerVisitor {
public:
    JsonSerializerVisitor(json& j) : j {j} {}

    auto operator()(Pass) const { return CALL_PASS_TAG; }
    auto operator()(const Bid& bid) const
    {
        j[CALL_BID_TAG] = toJson(bid);
        return CALL_BID_TAG;
    }
    auto operator()(Double) const { return CALL_DOUBLE_TAG; }
    auto operator()(Redouble) const { return CALL_REDOUBLE_TAG; }

private:
    json& j;
};

const auto CALLS = std::map<std::string, std::function<Call(const json&)>> {
    { CALL_PASS_TAG, [](const json&) { return Call {Pass {}}; }},
    { CALL_BID_TAG,
          [](const json& j) { return Call {checkedGet<Bid>(j, CALL_BID_TAG)}; }
    },
    { CALL_DOUBLE_TAG, [](const json&) { return Call {Double {}}; }},
    { CALL_REDOUBLE_TAG, [](const json&) { return Call {Redouble {}}; }}};

}

json JsonConverter<Call>::convertToJson(const Call& call)
{
    json j = json::object();
    j[CALL_TYPE_KEY] = boost::apply_visitor(JsonSerializerVisitor {j}, call);
    return j;
}

Call JsonConverter<Call>::convertFromJson(const nlohmann::json& j)
{
    const auto iter = CALLS.find(checkedGet<std::string>(j, CALL_TYPE_KEY));
    if (iter != CALLS.end()) {
        return iter->second(j);
    }
    throw MessageHandlingException {};
}

}
}
