#include "main/BridgeMain.hh"

#include <zmq.hpp>

#include <cstdlib>

int main()
{
    zmq::context_t zmqctx;
    Bridge::Main::BridgeMain app {
        zmqctx, "tcp://127.0.0.1:5555", "tcp://127.0.0.1:5556"};
    app.run();
    return EXIT_SUCCESS;
}
