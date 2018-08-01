#include "messaging/Security.hh"

namespace Bridge {
namespace Messaging {

namespace {

void setKey(zmq::socket_t& socket, int opt, const std::string& key)
{
    if (key.size() == EXPECTED_CURVE_KEY_SIZE) {
        socket.setsockopt(opt, key.c_str(), EXPECTED_CURVE_KEY_SIZE + 1);
    }
}

}

void setupCurveServer(zmq::socket_t& socket, const CurveKeys* const keys)
{
    if (keys) {
        socket.setsockopt(ZMQ_CURVE_SERVER, 1);
        setKey(socket, ZMQ_CURVE_SECRETKEY, keys->secretKey);
    }
}

void setupCurveClient(zmq::socket_t& socket, const CurveKeys* const keys)
{
    if (keys) {
        socket.setsockopt(ZMQ_CURVE_SERVER, 0);
        setKey(socket, ZMQ_CURVE_SERVERKEY, keys->serverKey);
        setKey(socket, ZMQ_CURVE_SECRETKEY, keys->secretKey);
        setKey(socket, ZMQ_CURVE_PUBLICKEY, keys->publicKey);
    }
}

}
}
