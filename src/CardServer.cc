#include "messaging/Security.hh"
#include "csmain/CardServerMain.hh"
#include "Logging.hh"

#include <getopt.h>
#include <libTMCG.hh>
#include <zmq.hpp>

#include <array>
#include <optional>
#include <string>

namespace {

using namespace Bridge;
using CardServer::CardServerMain;

class CardServerApp {
public:

    CardServerApp(
        zmq::context_t& zmqctx,
        std::optional<Messaging::CurveKeys> keys,
        const std::string& controlEndpoint,
        const std::string& basePeerEndpoint) :
        app {zmqctx, std::move(keys), controlEndpoint, basePeerEndpoint}
    {
        log(Bridge::LogLevel::INFO, "Setup completed");
    }

    ~CardServerApp()
    {
        log(Bridge::LogLevel::INFO, "Shutting down");
    }

    void run()
    {
        app.run();
    }

private:

    CardServerMain app;
};

}

CardServerApp createApp(zmq::context_t& zmqctx, int argc, char* argv[])
{
    auto curveSecretKey = std::string {};
    auto curvePublicKey = std::string {};

    const auto short_opt = "vs:p:";
    const auto long_opt = std::array {
        option { "curve-secret-key", required_argument, 0, 's' },
        option { "curve-public-key", required_argument, 0, 'p' },
        option { nullptr, 0, 0, 0 },
    };
    auto verbosity = 0;
    auto opt_index = 0;
    while(true) {
        auto c = getopt_long(
            argc, argv, short_opt, long_opt.data(), &opt_index);
        if (c == -1 || c == '?') {
            break;
        } else if (c == 'v') {
            ++verbosity;
        } else if (c == 's') {
            curveSecretKey = optarg;
        } else if (c == 'p') {
            curvePublicKey = optarg;
        } else {
            std::exit(EXIT_FAILURE);
        }
    }

    if (optind + 1 >= argc) {
        std::cerr << argv[0]
            << ": control and base peer endpoints required" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    if (curveSecretKey.empty() != curvePublicKey.empty()) {
        std::cerr << argv[0]
            << ": both curve secret and public key must be provided, or neither"
            << std::endl;
        std::exit(EXIT_FAILURE);
    }

    setupLogging(Bridge::getLogLevel(verbosity), std::cerr);

    if (!init_libTMCG()) {
        log(Bridge::LogLevel::FATAL,
            "Failed to initialize LibTMCG. Exiting.");
        std::exit(EXIT_FAILURE);
    }

    auto keys = curveSecretKey.empty() ? std::nullopt :
        std::make_optional(
            Messaging::CurveKeys {
                curvePublicKey, curveSecretKey, curvePublicKey });
    return CardServerApp {
        zmqctx, std::move(keys), argv[optind], argv[optind+1]};
}

int bridge_main(int argc, char* argv[])
{
    zmq::context_t zmqctx;
    createApp(zmqctx, argc, argv).run();
    return EXIT_SUCCESS;
}
