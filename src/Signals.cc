#include <signal.h>

#include <cstdlib>
#include <iostream>

namespace Bridge {

namespace {

void setSigactionOrDie(
    const int signum, const struct sigaction* const action)
{
    if (sigaction(signum, action, nullptr)) {
        std::cerr << "failed to set signal handler" << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

void setSignalHandler(const int signum, void (*handler)(int))
{
    struct sigaction action;
    action.sa_handler = handler;
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

}

void startHandlingSignals(void (*handler)(int))
{
    setSignalHandler(SIGINT, handler);
    setSignalHandler(SIGTERM, handler);
}

void stopHandlingSignals()
{
    resetSignalHandler(SIGTERM);
    resetSignalHandler(SIGINT);
}

}
