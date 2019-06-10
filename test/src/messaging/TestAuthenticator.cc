#include "messaging/Authenticator.hh"
#include "messaging/MessageUtility.hh"
#include "messaging/TerminationGuard.hh"
#include "messaging/Security.hh"

#include <gtest/gtest.h>
#include <optional>

using Bridge::Blob;
using Bridge::asBytes;
using namespace Bridge::Messaging;

namespace {
using namespace std::string_literals;
using namespace Bridge::BlobLiterals;
const auto ZAP_ENDPOINT = "inproc://zeromq.zap.01";
const auto ENDPOINT = "tcp://127.0.0.1:5555";
const auto ZAP_DOMAIN = "test"_B;
const auto SERVER_PUBLIC_KEY = decodeKey("rq:rM>}U?@Lns47E1%kR.o@n%FcmmsL/@{H8]yf7");
const auto SERVER_SECRET_KEY = decodeKey("JTKVSB%%)wK0E.X)V>+}o?pNmC{O&4W4b!Ni{Lh6");
const auto CLIENT_PUBLIC_KEY = decodeKey("Yne@$w-vo<fVvi]a<NY6T1ed:M$fCG*[IaLV{hID");
const auto CLIENT_SECRET_KEY = decodeKey("D:)Q[IlAW!ahhC2ac:9*A}h:p?([4%wOTJ%JR%cs");
const auto CLIENT_USER_ID = "user"s;
const auto CLIENT2_PUBLIC_KEY = decodeKey("}Nd:*=$4Fvzi5ehoQw/ew8tZ/XKI.C8o5YBqJcMR");
const auto CLIENT2_SECRET_KEY = decodeKey("G-Lq6{EbJ/C</gpvtK3V:4Sx[hsdePYi7[]4a3Nx");
const auto CLIENT2_USER_ID = "user2"s;
const auto ZAP_VERSION = "1.0"s;
const auto ZAP_REQUEST_ID = "testreq"s;
const auto CURVE_MECHANISM = "CURVE"s;
}

class AuthenticatorTest : public testing::Test {
protected:
    AuthenticatorTest()
    {
        const auto keys = CurveKeys {SERVER_SECRET_KEY, SERVER_PUBLIC_KEY};
        server.setsockopt(ZMQ_ZAP_DOMAIN, ZAP_DOMAIN.data(), ZAP_DOMAIN.size());
        setupCurveServer(server, &keys);
        authenticator.ensureRunning();
        server.bind(ENDPOINT);
        zap_client.connect(ZAP_ENDPOINT);
    }

    void setupClient(const CurveKeys& keys)
    {
        setupCurveClient(client, &keys, SERVER_PUBLIC_KEY);
        client.connect(ENDPOINT);
    }

    auto recvClientUserId()
    {
        client.send("", 0);
        auto message = zmq::message_t {};
        server.recv(&message);
        const auto ret = std::string(message.gets("User-Id"));
        server.send("", 0);
        client.recv(&message);
        return ret;
    }

    void testZapReply(
        bool success,
        std::optional<std::string> expectedRequestId = std::nullopt,
        std::optional<std::string> expectedUserId = std::nullopt)
    {
        auto message = zmq::message_t {};
        zap_client.recv(&message);
        EXPECT_EQ(asBytes(ZAP_VERSION), messageView(message));
        ASSERT_TRUE(message.more());
        zap_client.recv(&message);
        if (expectedRequestId) {
            EXPECT_EQ(asBytes(*expectedRequestId), messageView(message));
        }
        ASSERT_TRUE(message.more());
        zap_client.recv(&message);
        ASSERT_GE(message.size(), 1u);
        EXPECT_EQ(success, *message.data<char>() == '2');
        ASSERT_TRUE(message.more());
        zap_client.recv(&message);
        // ignore status text
        ASSERT_TRUE(message.more());
        zap_client.recv(&message);
        if (expectedUserId) {
            EXPECT_EQ(asBytes(*expectedUserId), messageView(message));
        }
        ASSERT_TRUE(message.more());
        zap_client.recv(&message);
        // ignore metadata
        ASSERT_FALSE(message.more());
    }

    zmq::context_t context;
    Bridge::Messaging::Authenticator authenticator {
        context, TerminationGuard::createTerminationSubscriber(context),
        {
            { CLIENT_PUBLIC_KEY, CLIENT_USER_ID },
        }};
    zmq::socket_t server {context, zmq::socket_type::rep};
    zmq::socket_t client {context, zmq::socket_type::req};
    zmq::socket_t zap_client {context, zmq::socket_type::req};
    TerminationGuard terminationGuard {context};
};

TEST_F(AuthenticatorTest, testAuthenticationKnownPeer)
{
    setupClient({CLIENT_SECRET_KEY, CLIENT_PUBLIC_KEY});
    const auto user_id = recvClientUserId();
    EXPECT_EQ(CLIENT_USER_ID, user_id);
}

TEST_F(AuthenticatorTest, testAuthenticationUnknownPeer)
{
    setupClient({CLIENT2_SECRET_KEY, CLIENT2_PUBLIC_KEY});
    const auto user_id = recvClientUserId();
    EXPECT_NE(CLIENT2_USER_ID, user_id);
}

TEST_F(AuthenticatorTest, testAuthenticationNewPeer)
{
    authenticator.addNode(CLIENT2_PUBLIC_KEY, CLIENT2_USER_ID);
    setupClient({CLIENT2_SECRET_KEY, CLIENT2_PUBLIC_KEY});
    const auto user_id = recvClientUserId();
    EXPECT_EQ(CLIENT2_USER_ID, user_id);
}

TEST_F(AuthenticatorTest, testZapRequest)
{
    zap_client.send(ZAP_VERSION.data(), ZAP_VERSION.size(), ZMQ_SNDMORE);
    zap_client.send(ZAP_REQUEST_ID.data(), ZAP_REQUEST_ID.size(), ZMQ_SNDMORE);
    zap_client.send(ZAP_DOMAIN.data(), ZAP_DOMAIN.size(), ZMQ_SNDMORE);
    zap_client.send("", 0, ZMQ_SNDMORE); // address
    zap_client.send("", 0, ZMQ_SNDMORE); // id
    zap_client.send(
        CURVE_MECHANISM.data(), CURVE_MECHANISM.size(), ZMQ_SNDMORE);
    zap_client.send(CLIENT_PUBLIC_KEY.data(), CLIENT_PUBLIC_KEY.size());
    testZapReply(true, ZAP_REQUEST_ID, CLIENT_USER_ID);
}

TEST_F(AuthenticatorTest, testZapRequestInvalidMessageSize)
{
    zap_client.send(ZAP_VERSION.data(), ZAP_VERSION.size(), 0);
    testZapReply(false);
}

TEST_F(AuthenticatorTest, testZapRequestInvalidVersion)
{
    zap_client.send("", 0, ZMQ_SNDMORE); // version
    zap_client.send(ZAP_REQUEST_ID.data(), ZAP_REQUEST_ID.size(), ZMQ_SNDMORE);
    zap_client.send(ZAP_DOMAIN.data(), ZAP_DOMAIN.size(), ZMQ_SNDMORE);
    zap_client.send("", 0, ZMQ_SNDMORE); // address
    zap_client.send("", 0, ZMQ_SNDMORE); // id
    zap_client.send(
        CURVE_MECHANISM.data(), CURVE_MECHANISM.size(), ZMQ_SNDMORE);
    zap_client.send(CLIENT_PUBLIC_KEY.data(), CLIENT_PUBLIC_KEY.size());
    testZapReply(false, ZAP_REQUEST_ID);
}

TEST_F(AuthenticatorTest, testZapRequestInvalidMechanism)
{
    zap_client.send(ZAP_VERSION.data(), ZAP_VERSION.size(), ZMQ_SNDMORE);
    zap_client.send(ZAP_REQUEST_ID.data(), ZAP_REQUEST_ID.size(), ZMQ_SNDMORE);
    zap_client.send(ZAP_DOMAIN.data(), ZAP_DOMAIN.size(), ZMQ_SNDMORE);
    zap_client.send("", 0, ZMQ_SNDMORE); // address
    zap_client.send("", 0, ZMQ_SNDMORE); // id
    zap_client.send("", 0, ZMQ_SNDMORE); // mechanism
    zap_client.send(CLIENT_PUBLIC_KEY.data(), CLIENT_PUBLIC_KEY.size());
    testZapReply(false, ZAP_REQUEST_ID);
}
