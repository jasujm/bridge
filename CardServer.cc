#include "csmain/CardServerMain.hh"

#include <libTMCG.hh>
#include <zmq.hpp>

#include <signal.h>

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

void setSigactionOrDie(
    const int signum, const struct sigaction* const action)
{
    if (sigaction(signum, action, nullptr)) {
        std::cerr << "failed to set signal handler" << std::endl;
        exit(EXIT_FAILURE);
    }
}

void setSignalHandler(const int signum)
{
    struct sigaction action;
    action.sa_handler = &signalHandler;
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGTERM);
    sigaddset(&action.sa_mask, SIGINT);
    action.sa_flags = 0;
    setSigactionOrDie(signum, &action);
}

void resetSignalHandler(const int signum)
{
    struct sigaction action;
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    setSigactionOrDie(signum, &action);
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
        setSignalHandler(SIGINT);
        setSignalHandler(SIGTERM);
    }

    ~CardServerApp()
    {
        appObserver = nullptr;
        resetSignalHandler(SIGTERM);
        resetSignalHandler(SIGINT);
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
