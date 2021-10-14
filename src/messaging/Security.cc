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

template<typename Opt>
void setKey(Socket& socket, Opt opt, ByteSpan key)
{
    assert (key.size() == EXPECTED_CURVE_KEY_SIZE);
    socket.set(opt, messageBuffer(key));
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
            socket.set(zmq::sockopt::curve_server, true);
            setKey(socket, zmq::sockopt::curve_secretkey, keys->secretKey);
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
            socket.set(zmq::sockopt::curve_server, false);
            setKey(socket, zmq::sockopt::curve_serverkey, serverKey);
            setKey(socket, zmq::sockopt::curve_secretkey, keys->secretKey);
            setKey(socket, zmq::sockopt::curve_publickey, keys->publicKey);
        } else {
            throw std::invalid_argument {"Invalid client keys"};
        }
    }
}

}
}
