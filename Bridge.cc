#include "main/BridgeMain.hh"

#include "bridge/Position.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/PositionJsonSerializer.hh"

#include <zmq.hpp>

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <csignal>
#include <string>
#include <utility>
#include <vector>

namespace {

using PositionVector = std::vector<Bridge::Position>;
using StringVector = std::vector<std::string>;

std::atomic<Bridge::Main::BridgeMain*> appObserver {nullptr};

extern "C" void signalHandler(int)
{
    const auto app_observer = appObserver.load();
    assert(app_observer);
    app_observer->terminate();
}

struct BridgeApp {
public:

    BridgeApp(
        std::string controlEndpoint,
        std::string eventEndpoint,
        PositionVector positions,
        StringVector peers) :
        app {
            zmqctx, std::move(controlEndpoint), std::move(eventEndpoint),
            std::move(positions), std::move(peers)}
    {
        appObserver = &app;
        // TODO: This is not strictly portable as setting signal handler in
        // multithreaded application is not defined by the C++ standard
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
    }

    ~BridgeApp()
    {
        std::signal(SIGTERM, SIG_DFL);
        std::signal(SIGINT, SIG_DFL);
        appObserver = nullptr;
    }

    void run()
    {
        app.run();
    }

private:

    zmq::context_t zmqctx;
    Bridge::Main::BridgeMain app;
};

template<typename T>
T parseArgument(const char* arg)
{
    auto serializer = Bridge::Messaging::JsonSerializer {};
    return serializer.deserialize<T>(arg);
}

}

int main(int argc, char* argv[])
{
    if (argc != 5) {
        return EXIT_FAILURE;
    }

    BridgeApp app {
        argv[1],
        argv[2],
        parseArgument<PositionVector>(argv[3]),
        parseArgument<StringVector>(argv[4])};
    app.run();
    return EXIT_SUCCESS;
}
