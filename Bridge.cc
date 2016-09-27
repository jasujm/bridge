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

namespace {

using Bridge::Main::BridgeMain;

std::atomic<BridgeMain*> appObserver {nullptr};

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
        const std::string& baseEndpoint,
        const BridgeMain::PositionVector& positions,
        const BridgeMain::EndpointVector& peerEndpoints,
        const std::string& cardServerControlEndpoint,
        const std::string& cardServerBasePeerEndpoint) :
        app {
            zmqctx, baseEndpoint, positions, peerEndpoints,
            cardServerControlEndpoint, cardServerBasePeerEndpoint}
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

    BridgeMain app;
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
    auto positions = BridgeMain::PositionVector(
        POSITIONS.begin(), POSITIONS.end());
    auto peerEndpoints = BridgeMain::EndpointVector {};
    auto cardServerControlEndpoint = std::string {};
    auto cardServerBasePeerEndpoint = std::string {};

    const auto short_opt = "b:p:c:t:q:";
    std::array<struct option, 6> long_opt {{
        { "bind", required_argument, 0, 'b' },
        { "positions", required_argument, 0, 'p' },
        { "connect", required_argument, 0, 'c' },
        { "cs-cntl", required_argument, 0, 't' },
        { "cs-peer", required_argument, 0, 'q' },
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
            positions = parseArgument<BridgeMain::PositionVector>(optarg);
        } else if (c == 'c') {
            peerEndpoints = parseArgument<BridgeMain::EndpointVector>(optarg);
        } else if (c == 't') {
            cardServerControlEndpoint = optarg;
        } else if (c == 'q') {
            cardServerBasePeerEndpoint = optarg;
        } else {
            exit(EXIT_FAILURE);
        }
    }

    if (baseEndpoint.empty()) {
        std::cerr << argv[0] << ": --bind option required" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    return BridgeApp {
        zmqctx, baseEndpoint, positions, peerEndpoints,
        cardServerControlEndpoint, cardServerBasePeerEndpoint};
}

}

int main(int argc, char* argv[])
{
    zmq::context_t zmqctx;
    createApp(zmqctx, argc, argv).run();
    return EXIT_SUCCESS;
}
