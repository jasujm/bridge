#include "bridge/Position.hh"
#include "main/BridgeMain.hh"
#include "main/Config.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "Logging.hh"

#include <zmq.hpp>
#include <getopt.h>

#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using namespace Bridge;
using Main::BridgeMain;
using Messaging::JsonSerializer;

class BridgeApp {
public:

    BridgeApp(
        zmq::context_t& zmqctx,
        const std::string& configPath,
        const std::string& baseEndpoint,
        BridgeMain::PositionVector positions,
        const BridgeMain::EndpointVector& peerEndpoints,
        const std::string& cardServerControlEndpoint,
        const std::string& cardServerBasePeerEndpoint) :
        app {
            zmqctx, Main::configFromPath(configPath), baseEndpoint,
            std::move(positions), peerEndpoints, cardServerControlEndpoint,
            cardServerBasePeerEndpoint}
    {
        log(Bridge::LogLevel::INFO, "Startup completed");
    }

    ~BridgeApp()
    {
        log(Bridge::LogLevel::INFO, "Shutting down");
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
    auto configPath = std::string {};
    auto positions = BridgeMain::PositionVector {};
    auto peerEndpoints = BridgeMain::EndpointVector {};
    auto cardServerControlEndpoint = std::string {};
    auto cardServerBasePeerEndpoint = std::string {};

    const auto short_opt = "vf:p:c:t:q:";
    auto long_opt = std::array {
        option { "config", required_argument, 0, 'f' },
        option { "positions", required_argument, 0, 'p' },
        option { "connect", required_argument, 0, 'c' },
        option { "cs-cntl", required_argument, 0, 't' },
        option { "cs-peer", required_argument, 0, 'q' },
        option { nullptr, 0, 0, 0 },
    };
    auto verbosity = 0;
    auto opt_index = 0;
    while (true) {
        auto c = getopt_long(
            argc, argv, short_opt, long_opt.data(), &opt_index);
        if (c == -1 || c == '?') {
            break;
        } else if (c == 'v') {
            ++verbosity;
        } else if (c == 'f') {
            configPath = optarg;
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

    if (optind == argc) {
        std::cerr << argv[0] << ": base endpoint required" << std::endl;
        exit(EXIT_FAILURE);
    }

    setupLogging(Bridge::getLogLevel(verbosity), std::cerr);

    return BridgeApp {
        zmqctx, std::move(configPath), argv[optind], std::move(positions),
        peerEndpoints, cardServerControlEndpoint, cardServerBasePeerEndpoint};
}

}

int bridge_main(int argc, char* argv[])
{
    zmq::context_t zmqctx;
    createApp(zmqctx, argc, argv).run();
    return EXIT_SUCCESS;
}
