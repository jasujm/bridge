#include "csmain/CardServerMain.hh"
#include "Signals.hh"

#include <libTMCG.hh>
#include <zmq.hpp>

#include <atomic>
#include <string>

namespace {

using Bridge::CardServer::CardServerMain;

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
    }

    ~CardServerApp()
    {
        stopHandlingSignals();
        appObserver = nullptr;
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
    if (argc != 3) {
        return EXIT_FAILURE;
    }

    if (!init_libTMCG()) {
        return EXIT_FAILURE;
    }

    zmq::context_t zmqctx;
    CardServerApp {zmqctx, argv[1], argv[2]}.run();
    return EXIT_SUCCESS;
}
