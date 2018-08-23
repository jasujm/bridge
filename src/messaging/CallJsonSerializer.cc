#include "messaging/CallJsonSerializer.hh"

#include "messaging/BidJsonSerializer.hh"
#include "messaging/SerializationFailureException.hh"

#include <functional>
#include <map>

using nlohmann::json;

namespace Bridge {

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
        j[CALL_BID_TAG] = bid;
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
      [](const json& j) { return Call {j.at(CALL_BID_TAG).get<Bid>()}; }
    },
    { CALL_DOUBLE_TAG, [](const json&) { return Call {Double {}}; }},
    { CALL_REDOUBLE_TAG, [](const json&) { return Call {Redouble {}}; }}};

}

void to_json(json& j, const Call& call)
{
    j[CALL_TYPE_KEY] = std::visit(JsonSerializerVisitor {j}, call);
}

void from_json(const nlohmann::json& j, Call& call)
{
    const auto iter = CALLS.find(j.at(CALL_TYPE_KEY));
    if (iter != CALLS.end()) {
        call = iter->second(j);
    } else {
        throw Messaging::SerializationFailureException {};
    }
}

}
