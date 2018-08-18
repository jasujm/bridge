#include "messaging/Security.hh"
#include "csmain/CardServerMain.hh"
#include "IoUtility.hh"
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

auto getKeyFromFile(
    const std::string_view path, const std::string_view prog,
    const std::string_view name)
{
    errno = 0;
    return processStreamFromPath(
        path,
        [prog, name](auto& in)
        {
            auto key = std::string {};
            if (in.bad()) {
                std::cerr
                    << prog << ": bad stream while reading " << name
                    << ": " << strerror(errno) << std::endl;
                std::exit(EXIT_FAILURE);
            }
            in.clear();
            in >> key;
            if (!in.good()) {
                std::cerr << prog << ": failed to read " << name << std::endl;
                std::exit(EXIT_FAILURE);
            }
            auto ret = Messaging::decodeKey(key);
            if (ret.empty()) {
                std::cerr << prog << ": invalid " << name << std::endl;
                std::exit(EXIT_FAILURE);
            }
            return ret;
        });
}

}

CardServerApp createApp(zmq::context_t& zmqctx, int argc, char* argv[])
{
    auto curveSecretKey = Blob {};
    auto curvePublicKey = Blob {};

    const auto short_opt = "vs:p:";
    const auto long_opt = std::array {
        option { "secret-key-file", required_argument, 0, 's' },
        option { "public-key-file", required_argument, 0, 'p' },
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
            curveSecretKey = getKeyFromFile(optarg, argv[0], "secret key");
        } else if (c == 'p') {
            curvePublicKey = getKeyFromFile(optarg, argv[0], "public key");
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
