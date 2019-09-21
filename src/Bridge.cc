#include "messaging/Sockets.hh"
#include "main/BridgeMain.hh"
#include "main/Config.hh"
#include "Logging.hh"

#include <getopt.h>

#include <array>
#include <cassert>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {

using namespace Bridge;
using Main::BridgeMain;

class BridgeApp {
public:

    BridgeApp(
        Messaging::MessageContext& zmqctx,
        const std::string& configPath) :
        app {zmqctx, Main::configFromPath(configPath)}
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

BridgeApp createApp(Messaging::MessageContext& zmqctx, int argc, char* argv[])
{
    auto configPath = std::string {};

    const auto short_opt = "vf:p:c:t:q:";
    auto long_opt = std::array {
        option { "config", required_argument, 0, 'f' },
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
        } else {
            std::exit(EXIT_FAILURE);
        }
    }

    setupLogging(Bridge::getLogLevel(verbosity), std::cerr);

    return BridgeApp {zmqctx, std::move(configPath)};
}

}

int bridge_main(int argc, char* argv[])
{
    Messaging::MessageContext zmqctx;
    createApp(zmqctx, argc, argv).run();
    return EXIT_SUCCESS;
}
