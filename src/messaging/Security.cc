#include "messaging/Security.hh"

namespace Bridge {
namespace Messaging {

namespace {

bool checkKeys(const auto&... keys)
{
    return ( ... && (keys.size() == EXPECTED_CURVE_KEY_SIZE) );
}

void setKey(zmq::socket_t& socket, int opt, ByteSpan key)
{
    assert (key.size() == EXPECTED_CURVE_KEY_SIZE);
    socket.setsockopt(opt, key.data(), EXPECTED_CURVE_KEY_SIZE);
}

}

Blob decodeKey(const std::string_view encodedKey)
{
    if (4 * encodedKey.size() == 5 * EXPECTED_CURVE_KEY_SIZE) {
        auto ret = Blob(EXPECTED_CURVE_KEY_SIZE, std::byte {});
        const auto success = zmq_z85_decode(
            reinterpret_cast<std::uint8_t*>(ret.data()), encodedKey.data());
        if (success) {
            return ret;
        }
    }
    return {};
}

void setupCurveServer(zmq::socket_t& socket, const CurveKeys* const keys)
{
    if (keys && checkKeys(keys->secretKey)) {
        socket.setsockopt(ZMQ_CURVE_SERVER, 1);
        setKey(socket, ZMQ_CURVE_SECRETKEY, keys->secretKey);
    }
}

void setupCurveClient(
    zmq::socket_t& socket, const CurveKeys* const keys, ByteSpan serverKey)
{
    if (keys && checkKeys(keys->secretKey, keys->publicKey) &&
        serverKey.size() > 0) {
        socket.setsockopt(ZMQ_CURVE_SERVER, 0);
        setKey(socket, ZMQ_CURVE_SERVERKEY, serverKey);
        setKey(socket, ZMQ_CURVE_SECRETKEY, keys->secretKey);
        setKey(socket, ZMQ_CURVE_PUBLICKEY, keys->publicKey);
    }
}

}
}
