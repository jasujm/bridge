#include "messaging/Security.hh"

#include <stdexcept>

namespace Bridge {
namespace Messaging {

namespace {

template<typename... Keys>
bool checkKeys(const Keys&... keys)
{
    return ( ... && (keys.size() == EXPECTED_CURVE_KEY_SIZE) );
}

void setKey(Socket& socket, int opt, ByteSpan key)
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

void setupCurveServer(Socket& socket, const CurveKeys* const keys)
{
    if (keys) {
        if (checkKeys(keys->secretKey)) {
            socket.setsockopt(ZMQ_CURVE_SERVER, 1);
            setKey(socket, ZMQ_CURVE_SECRETKEY, keys->secretKey);
        } else {
            throw std::invalid_argument {"Invalid server keys"};
        }
    }
}

void setupCurveClient(
    Socket& socket, const CurveKeys* const keys, ByteSpan serverKey)
{
    if (keys && serverKey.size() > 0) {
        if (checkKeys(keys->secretKey, keys->publicKey, serverKey)) {
            socket.setsockopt(ZMQ_CURVE_SERVER, 0);
            setKey(socket, ZMQ_CURVE_SERVERKEY, serverKey);
            setKey(socket, ZMQ_CURVE_SECRETKEY, keys->secretKey);
            setKey(socket, ZMQ_CURVE_PUBLICKEY, keys->publicKey);
        } else {
            throw std::invalid_argument {"Invalid client keys"};
        }
    }
}

}
}
