#include "cardserver/CardServerMain.hh"

#include "messaging/FunctionMessageHandler.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"

#include <boost/optional/optional.hpp>
#include <zmq.hpp>

#include <string>
#include <utility>
#include <vector>

namespace Bridge {

namespace CardServer {
using PeerEntry = std::pair<std::string, std::string>;
using PeerVector = std::vector<boost::optional<PeerEntry>>;
}

namespace Messaging {
// TODO: Move this to a common place when both serialization and
// deserialization are needed
template<>
struct JsonConverter<CardServer::PeerEntry> {
    static CardServer::PeerEntry convertFromJson(const nlohmann::json& j) {
        return jsonToPair<std::string, std::string>(
            j, "identity", "endpoint");
    }
};
}

namespace CardServer {

using Messaging::failure;
using Messaging::JsonSerializer;
using Messaging::makeMessageHandler;
using Messaging::Reply;
using Messaging::success;

const std::string CardServerMain::INIT_COMMAND {"init"};
const std::string CardServerMain::TERMINATE_COMMAND {"terminate"};

class CardServerMain::Impl {
public:
    Impl(zmq::context_t& context, const std::string& controlEndpoint);
    void run();

private:

    Reply<> init(const std::string& identity, const PeerVector& peers);

    zmq::socket_t controlSocket;
    Messaging::MessageQueue messageQueue;
};

CardServerMain::Impl::Impl(
    zmq::context_t& context, const std::string& controlEndpoint) :
    controlSocket {context, zmq::socket_type::pair},
    messageQueue {
        {
            {
                INIT_COMMAND,
                makeMessageHandler(*this, &Impl::init, JsonSerializer {})
            },
        },
        TERMINATE_COMMAND
    }
{
    controlSocket.bind(controlEndpoint);
}

void CardServerMain::Impl::run()
{
    while (messageQueue(controlSocket));
}

Reply<> CardServerMain::Impl::init(const std::string&, const PeerVector&)
{
    return success();
}

CardServerMain::CardServerMain(
    zmq::context_t& context, const std::string& controlEndpoint) :
    impl {std::make_unique<Impl>(context, controlEndpoint)}
{
}

CardServerMain::~CardServerMain() = default;

void CardServerMain::run()
{
    assert(impl);
    impl->run();
}

}
}
