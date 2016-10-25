#include "main/BridgeMain.hh"

#include "bridge/Position.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "Logging.hh"
#include "Signals.hh"

#include <boost/core/demangle.hpp>
#include <zmq.hpp>

#include <getopt.h>

#include <array>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

namespace {

using namespace Bridge;
using Main::BridgeMain;
using Messaging::JsonSerializer;

std::atomic<BridgeMain*> appObserver {nullptr};

extern "C" void signalHandler(int)
{
    const auto app_observer = appObserver.load();
    assert(app_observer);
    app_observer->terminate();
}

class BridgeApp {
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
        startHandlingSignals(signalHandler);
        log(Bridge::LogLevel::INFO, "Configuring application completed");
    }

    ~BridgeApp()
    {
        stopHandlingSignals();
        appObserver = nullptr;
        log(Bridge::LogLevel::INFO, "Preparing to shut down");
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
    auto serializer = JsonSerializer {};
    return serializer.deserialize<T>(arg);
}

BridgeApp createApp(zmq::context_t& zmqctx, int argc, char* argv[])
{
    auto baseEndpoint = std::string {};
    auto positions = BridgeMain::PositionVector(
        POSITIONS.begin(), POSITIONS.end());
    auto peerEndpoints = BridgeMain::EndpointVector {};
    auto cardServerControlEndpoint = std::string {};
    auto cardServerBasePeerEndpoint = std::string {};

    const auto short_opt = "vb:p:c:t:q:";
    std::array<struct option, 6> long_opt {{
        { "bind", required_argument, 0, 'b' },
        { "positions", required_argument, 0, 'p' },
        { "connect", required_argument, 0, 'c' },
        { "cs-cntl", required_argument, 0, 't' },
        { "cs-peer", required_argument, 0, 'q' },
        { nullptr, 0, 0, 0 },
    }};
    auto verbosity = 0;
    auto opt_index = 0;
    while (true) {
        auto c = getopt_long(
            argc, argv, short_opt, long_opt.data(), &opt_index);
        if (c == -1 || c == '?') {
            break;
        } else if (c == 'v') {
            ++verbosity;
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

    auto logging_level = Bridge::LogLevel::WARNING;
    if (verbosity == 1) {
        logging_level = Bridge::LogLevel::INFO;
    } else if (verbosity >= 2) {
        logging_level = Bridge::LogLevel::DEBUG;
    }
    setupLogging(logging_level, std::cerr);

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
#ifndef NDEBUG
    setupLogging(Bridge::LogLevel::DEBUG, std::cerr);
#endif
    log(Bridge::LogLevel::INFO, "%s started", argv[0]);
    try {
        zmq::context_t zmqctx;
        createApp(zmqctx, argc, argv).run();
    } catch (std::exception& e) {
        log(
            Bridge::LogLevel::FATAL,
            "%s terminated with exception of type %r: %s",
            argv[0], boost::core::demangle(typeid(e).name()), e.what());
    }
    return EXIT_SUCCESS;
}
