#include "csmain/CardServerMain.hh"
#include "Logging.hh"

#include <getopt.h>
#include <libTMCG.hh>
#include <zmq.hpp>

#include <string>

namespace {

using namespace Bridge;
using CardServer::CardServerMain;

class CardServerApp {
public:

    CardServerApp(
        zmq::context_t& zmqctx,
        const std::string& controlEndpoint,
        const std::string& basePeerEndpoint) :
        app {zmqctx, controlEndpoint, basePeerEndpoint}
    {
        log(Bridge::LogLevel::INFO, "Setup completed");
    }

    CardServerApp(CardServerApp&&) = default;

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
    const auto opt = "v";
    auto verbosity = 0;
    while(true) {
        auto c = getopt(argc, argv, opt);
        if (c == -1 || c == '?') {
            break;
        } else if (c == 'v') {
            ++verbosity;
        } else {
            exit(EXIT_FAILURE);
        }
    }

    if (optind + 1 >= argc) {
        std::cerr << argv[0]
            << ": control and base peer endpoints required" << std::endl;
        exit(EXIT_FAILURE);
    }

    setupLogging(Bridge::getLogLevel(verbosity), std::cerr);

    if (!init_libTMCG()) {
        log(Bridge::LogLevel::FATAL,
            "Failed to initialize LibTMCG. Exiting.");
        exit(EXIT_FAILURE);
    }

    return CardServerApp {zmqctx, argv[optind], argv[optind+1]};
}

int bridge_main(int argc, char* argv[])
{
    zmq::context_t zmqctx;
    createApp(zmqctx, argc, argv).run();
    return EXIT_SUCCESS;
}
