#include "main/BridgeMain.hh"

#include <zmq.hpp>

#include <cstdlib>
#include <csignal>

namespace {

Bridge::Main::BridgeMain* appObserver {nullptr};

void signalHandler(int)
{
    assert(appObserver);
    appObserver->terminate();
}

struct BridgeApp {
public:

    BridgeApp()
    {
        appObserver = &app;
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
    }

    ~BridgeApp()
    {
        std::signal(SIGTERM, SIG_DFL);
        std::signal(SIGINT, SIG_DFL);
        appObserver = nullptr;
    }

    void run()
    {
        app.run();
    }

private:

    zmq::context_t zmqctx;
    Bridge::Main::BridgeMain app {
        zmqctx, "tcp://127.0.0.1:5555", "tcp://127.0.0.1:5556"};
};

}

int main()
{
    BridgeApp app;
    app.run();
    return EXIT_SUCCESS;
}
