#include "cardserver/CardServerMain.hh"

#include <zmq.hpp>

int main(int argc, char* argv[])
{
    if (argc != 2) {
        return EXIT_FAILURE;
    }

    zmq::context_t zmqctx;
    Bridge::CardServer::CardServerMain app {zmqctx, argv[1]};
    app.run();
    return EXIT_SUCCESS;
}
