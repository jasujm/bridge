#include "messaging/Security.hh"

#include <gtest/gtest.h>
#include <zmq.hpp>

#include <stdexcept>

using Bridge::Messaging::CurveKeys;
using namespace Bridge::BlobLiterals;
using namespace Bridge::Messaging;

namespace {
const auto SERVER_PUBLIC_KEY = decodeKey("rq:rM>}U?@Lns47E1%kR.o@n%FcmmsL/@{H8]yf7");
const auto SERVER_SECRET_KEY = decodeKey("JTKVSB%%)wK0E.X)V>+}o?pNmC{O&4W4b!Ni{Lh6");
const auto CLIENT_PUBLIC_KEY = decodeKey("Yne@$w-vo<fVvi]a<NY6T1ed:M$fCG*[IaLV{hID");
const auto CLIENT_SECRET_KEY = decodeKey("D:)Q[IlAW!ahhC2ac:9*A}h:p?([4%wOTJ%JR%cs");
}

class SecurityTest : public testing::Test {
protected:
    zmq::context_t context;
    Socket socket {context, SocketType::req};
};

TEST_F(SecurityTest, testSetupServer)
{
    const auto keys = CurveKeys {SERVER_SECRET_KEY, SERVER_PUBLIC_KEY};
    setupCurveServer(socket, &keys);
    EXPECT_TRUE(socket.getsockopt<int>(ZMQ_CURVE_SERVER));
}

TEST_F(SecurityTest, testSetupCLIENT)
{
    const auto keys = CurveKeys {CLIENT_SECRET_KEY, CLIENT_PUBLIC_KEY};
    setupCurveClient(socket, &keys, SERVER_PUBLIC_KEY);
    EXPECT_FALSE(socket.getsockopt<int>(ZMQ_CURVE_SERVER));
}

TEST_F(SecurityTest, testSetupServerInvalidKeys)
{
    const auto keys = CurveKeys {"bogus"_B, "keys"_B};
    EXPECT_THROW(setupCurveServer(socket, &keys), std::invalid_argument);
}

TEST_F(SecurityTest, testSetupClientInvalidKeys)
{
    const auto keys = CurveKeys {"bogus"_B, "keys"_B};
    EXPECT_THROW(setupCurveClient(socket, &keys, SERVER_PUBLIC_KEY), std::invalid_argument);
}

TEST_F(SecurityTest, testSetupClientServerKey)
{
    const auto keys = CurveKeys {CLIENT_SECRET_KEY, CLIENT_PUBLIC_KEY};
    EXPECT_THROW(setupCurveClient(socket, &keys, "bogus key"_BS), std::invalid_argument);
}
