#include "main/BridgeMain.hh"

#include <zmq.hpp>

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <csignal>

namespace {

std::atomic<Bridge::Main::BridgeMain*> appObserver {nullptr};

extern "C" void signalHandler(int)
{
    const auto app_observer = appObserver.load();
    assert(app_observer);
    app_observer->terminate();
}

struct BridgeApp {
public:

    BridgeApp()
    {
        appObserver = &app;
        // TODO: This is not strictly portable as setting signal handler in
        // multithreaded application is not defined by the C++ standard
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
