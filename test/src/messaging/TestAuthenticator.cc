#include "messaging/Authenticator.hh"
#include "messaging/MessageUtility.hh"
#include "messaging/TerminationGuard.hh"
#include "messaging/Security.hh"
#include "messaging/Sockets.hh"

#include <gtest/gtest.h>
#include <optional>

using Bridge::Blob;
using Bridge::asBytes;
using namespace Bridge::Messaging;

namespace {
using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace Bridge::BlobLiterals;
constexpr auto ZAP_ENDPOINT = "inproc://zeromq.zap.01"sv;
constexpr auto ENDPOINT = "tcp://127.0.0.1:5555"sv;
constexpr auto ZAP_DOMAIN = "test"_BS;
const auto SERVER_PUBLIC_KEY = decodeKey("rq:rM>}U?@Lns47E1%kR.o@n%FcmmsL/@{H8]yf7");
const auto SERVER_SECRET_KEY = decodeKey("JTKVSB%%)wK0E.X)V>+}o?pNmC{O&4W4b!Ni{Lh6");
const auto CLIENT_PUBLIC_KEY = decodeKey("Yne@$w-vo<fVvi]a<NY6T1ed:M$fCG*[IaLV{hID");
const auto CLIENT_SECRET_KEY = decodeKey("D:)Q[IlAW!ahhC2ac:9*A}h:p?([4%wOTJ%JR%cs");
const auto CLIENT_USER_ID = "user"s;
const auto CLIENT2_PUBLIC_KEY = decodeKey("}Nd:*=$4Fvzi5ehoQw/ew8tZ/XKI.C8o5YBqJcMR");
const auto CLIENT2_SECRET_KEY = decodeKey("G-Lq6{EbJ/C</gpvtK3V:4Sx[hsdePYi7[]4a3Nx");
const auto CLIENT2_USER_ID = "user2"s;
constexpr auto ZAP_VERSION = "1.0"_BS;
constexpr auto ZAP_REQUEST_ID = "testreq"_BS;
constexpr auto CURVE_MECHANISM = "CURVE"_BS;
}

class AuthenticatorTest : public testing::Test {
protected:
    AuthenticatorTest()
    {
        const auto keys = CurveKeys {SERVER_SECRET_KEY, SERVER_PUBLIC_KEY};
        server.setsockopt(ZMQ_ZAP_DOMAIN, ZAP_DOMAIN.data(), ZAP_DOMAIN.size());
        setupCurveServer(server, &keys);
        authenticator.ensureRunning();
        bindSocket(server, ENDPOINT);
        connectSocket(zap_client, ZAP_ENDPOINT);
    }

    void setupClient(const CurveKeys& keys)
    {
        setupCurveClient(client, &keys, SERVER_PUBLIC_KEY);
        connectSocket(client, ENDPOINT);
    }

    auto recvClientUserId()
    {
        sendEmptyMessage(client);
        auto message = Message {};
        recvMessage(server, message);
        auto ret = UserId {message.gets("User-Id")};
        sendEmptyMessage(server);
        recvMessage(client, message);
        return ret;
    }

    void testZapReply(
        bool success,
        std::optional<Bridge::ByteSpan> expectedRequestId = std::nullopt,
        std::optional<std::string> expectedUserId = std::nullopt)
    {
        auto message = Message {};
        recvMessage(zap_client, message);
        EXPECT_EQ(asBytes(ZAP_VERSION), messageView(message));
        ASSERT_TRUE(message.more());
        recvMessage(zap_client, message);
        if (expectedRequestId) {
            EXPECT_EQ(asBytes(*expectedRequestId), messageView(message));
        }
        ASSERT_TRUE(message.more());
        recvMessage(zap_client, message);
        ASSERT_GE(message.size(), 1u);
        EXPECT_EQ(success, *message.data<char>() == '2');
        ASSERT_TRUE(message.more());
        recvMessage(zap_client, message);
        // ignore status text
        ASSERT_TRUE(message.more());
        recvMessage(zap_client, message);
        if (expectedUserId) {
            EXPECT_EQ(asBytes(*expectedUserId), messageView(message));
        }
        ASSERT_TRUE(message.more());
        recvMessage(zap_client, message);
        // ignore metadata
        ASSERT_FALSE(message.more());
    }

    MessageContext context;
    Authenticator authenticator {
        context, TerminationGuard::createTerminationSubscriber(context),
        {
            { CLIENT_PUBLIC_KEY, CLIENT_USER_ID },
        }};
    Socket server {context, SocketType::rep};
    Socket client {context, SocketType::req};
    Socket zap_client {context, SocketType::req};
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
    sendMessage(zap_client, messageBuffer(ZAP_VERSION), true);
    sendMessage(zap_client, messageBuffer(ZAP_REQUEST_ID), true);
    sendMessage(zap_client, messageBuffer(ZAP_DOMAIN), true);
    sendEmptyMessage(zap_client, true); // address
    sendEmptyMessage(zap_client, true); // id
    sendMessage(zap_client, messageBuffer(CURVE_MECHANISM), true);
    sendMessage(zap_client, messageBuffer(CLIENT_PUBLIC_KEY));
    testZapReply(true, ZAP_REQUEST_ID, CLIENT_USER_ID);
}

TEST_F(AuthenticatorTest, testZapRequestInvalidMessageSize)
{
    sendMessage(zap_client, messageBuffer(ZAP_VERSION));
    testZapReply(false);
}

TEST_F(AuthenticatorTest, testZapRequestInvalidVersion)
{
    sendEmptyMessage(zap_client, true); // version
    sendMessage(zap_client, messageBuffer(ZAP_REQUEST_ID), true);
    sendMessage(zap_client, messageBuffer(ZAP_DOMAIN), true);
    sendEmptyMessage(zap_client, true); // address
    sendEmptyMessage(zap_client, true); // id
    sendMessage(zap_client, messageBuffer(CURVE_MECHANISM), true);
    sendMessage(zap_client, messageBuffer(CLIENT_PUBLIC_KEY));
    testZapReply(false, ZAP_REQUEST_ID);
}

TEST_F(AuthenticatorTest, testZapRequestInvalidMechanism)
{
    sendMessage(zap_client, messageBuffer(ZAP_VERSION), true);
    sendMessage(zap_client, messageBuffer(ZAP_REQUEST_ID), true);
    sendMessage(zap_client, messageBuffer(ZAP_DOMAIN), true);
    sendEmptyMessage(zap_client, true); // address
    sendEmptyMessage(zap_client, true); // id
    sendEmptyMessage(zap_client, true); // mechanism
    sendMessage(zap_client, messageBuffer(CLIENT_PUBLIC_KEY));
    testZapReply(false, ZAP_REQUEST_ID);
}
