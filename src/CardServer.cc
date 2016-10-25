#include "csmain/CardServerMain.hh"
#include "Signals.hh"
#include "Logging.hh"

#include <boost/core/demangle.hpp>
#include <libTMCG.hh>
#include <zmq.hpp>

#include <atomic>
#include <exception>
#include <string>

namespace {

using namespace Bridge;
using CardServer::CardServerMain;

std::atomic<CardServerMain*> appObserver {nullptr};

extern "C" void signalHandler(int)
{
    const auto app_observer = appObserver.load();
    assert(app_observer);
    app_observer->terminate();
}

class CardServerApp {
public:

    CardServerApp(
        zmq::context_t& zmqctx,
        const std::string& controlEndpoint,
        const std::string& basePeerEndpoint) :
        app {zmqctx, controlEndpoint, basePeerEndpoint}
    {
        appObserver = &app;
        startHandlingSignals(signalHandler);
        log(Bridge::LogLevel::INFO, "Configuring application completed");
    }

    ~CardServerApp()
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

    CardServerMain app;
};

}

int main(int argc, char* argv[])
{
#ifndef NDEBUG
    setupLogging(Bridge::LogLevel::DEBUG, std::cerr);
#endif
   log(Bridge::LogLevel::INFO, "%s started", argv[0]);
    try {
        if (argc != 3) {
            std::cerr << argv[0] << ": Unexpected number of arguments" << std::endl;
            return EXIT_FAILURE;
        }

        if (!init_libTMCG()) {
            log(
                Bridge::LogLevel::FATAL,
                "Failed to initialize LibTMCG. Exiting.");
            return EXIT_FAILURE;
        }

        zmq::context_t zmqctx;
        CardServerApp {zmqctx, argv[1], argv[2]}.run();
    } catch (std::exception& e) {
        log(
            Bridge::LogLevel::FATAL,
            "%s terminated with exception of type %r: %s",
            argv[0], boost::core::demangle(typeid(e).name()), e.what());
    }
    return EXIT_SUCCESS;
}
