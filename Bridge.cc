#include "main/BridgeMain.hh"

#include "bridge/Position.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/PositionJsonSerializer.hh"

#include <zmq.hpp>

#include <getopt.h>
#include <signal.h>

#include <array>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <iostream>
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

void setSigactionOrDie(
    const int signum, const struct sigaction* const action)
{
    if (sigaction(signum, action, nullptr)) {
        std::cerr << "failed to set signal handler" << std::endl;
        exit(EXIT_FAILURE);
    }
}

void setSignalHandler(const int signum)
{
    struct sigaction action;
    action.sa_handler = &signalHandler;
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGTERM);
    sigaddset(&action.sa_mask, SIGINT);
    action.sa_flags = 0;
    setSigactionOrDie(signum, &action);
}

void resetSignalHandler(const int signum)
{
    struct sigaction action;
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    setSigactionOrDie(signum, &action);
}

struct BridgeApp {
public:

    BridgeApp(
        zmq::context_t& zmqctx,
        std::string baseEndpoint,
        PositionVector positions,
        StringVector peerEndpoints) :
        app {
            zmqctx, std::move(baseEndpoint),
            std::move(positions), std::move(peerEndpoints)}
    {
        appObserver = &app;
        setSignalHandler(SIGINT);
        setSignalHandler(SIGTERM);
    }

    ~BridgeApp()
    {
        resetSignalHandler(SIGTERM);
        resetSignalHandler(SIGINT);
        appObserver = nullptr;
    }

    void run()
    {
        app.run();
    }

private:

    Bridge::Main::BridgeMain app;
};

template<typename T>
T parseArgument(const char* arg)
{
    auto serializer = Bridge::Messaging::JsonSerializer {};
    return serializer.deserialize<T>(arg);
}

BridgeApp createApp(zmq::context_t& zmqctx, int argc, char* argv[])
{
    using Bridge::POSITIONS;

    auto baseEndpoint = std::string {};
    auto positions = PositionVector(POSITIONS.begin(), POSITIONS.end());
    auto peerEndpoints = StringVector {};

    const auto short_opt = "b:p:c:";
    std::array<struct option, 4> long_opt {{
        { "bind", required_argument, 0, 'b' },
        { "positions", required_argument, 0, 'p' },
        { "connect", required_argument, 0, 'c' },
        { nullptr, 0, 0, 0 },
    }};
    auto opt_index = 0;
    while (true) {
        auto c = getopt_long(
            argc, argv, short_opt, long_opt.data(), &opt_index);
        if (c == -1 || c == '?') {
            break;
        } else if (c == 'b') {
            baseEndpoint = optarg;
        } else if (c == 'p') {
            positions = parseArgument<PositionVector>(optarg);
        } else if (c == 'c') {
            peerEndpoints = parseArgument<StringVector>(optarg);
        } else {
            abort();
        }
    }

    if (baseEndpoint.empty()) {
        std::cerr << argv[0] << ": --bind option required" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    return BridgeApp {
        zmqctx, std::move(baseEndpoint), std::move(positions),
        std::move(peerEndpoints)};
}

}

int main(int argc, char* argv[])
{
    zmq::context_t zmqctx;
    createApp(zmqctx, argc, argv).run();
    return EXIT_SUCCESS;
}
