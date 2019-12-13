#include "bridge/Bid.hh"
#include "bridge/CardType.hh"
#include "bridge/Call.hh"
#include "bridge/Contract.hh"
#include "bridge/Partnership.hh"
#include "bridge/TricksWon.hh"
#include "bridge/Vulnerability.hh"
#include "cardserver/PeerEntry.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/BidJsonSerializer.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/ContractJsonSerializer.hh"
#include "messaging/DuplicateScoreJsonSerializer.hh"
#include "messaging/PeerEntryJsonSerializer.hh"
#include "messaging/Security.hh"
#include "messaging/SerializationFailureException.hh"
#include "messaging/TricksWonJsonSerializer.hh"
#include "messaging/UuidJsonSerializer.hh"
#include "messaging/VulnerabilityJsonSerializer.hh"
#include "scoring/DuplicateScore.hh"
#include "IoUtility.hh"

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <json.hpp>
#include <gtest/gtest.h>

#include <string_view>

using namespace Bridge;
using namespace Bridge::Messaging;
using Bridge::Scoring::DuplicateScore;

using nlohmann::json;

using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace Bridge::BlobLiterals;

namespace {
constexpr auto BID = Bid {4, Strains::HEARTS};
constexpr auto CONTRACT = Contract {BID, Doubling::DOUBLED};
constexpr auto VULNERABILITY = Vulnerability {true, false};
constexpr auto TRICKS_WON = TricksWon {5, 6};
const auto PEER_ENDPOINT = "inproc://test"s;
const auto PEER_SERVER_KEY = decodeKey(
    "rq:rM>}U?@Lns47E1%kR.o@n%FcmmsL/@{H8]yf7"sv);
}

class JsonSerializerTest : public testing::Test {
protected:

    template<typename T>
    void testHelper(const T& t, const json& j)
    {
        // Using EXPECT_EQ causes stack overflow is operations are
        // unsuccessful (because printing the error messages them would
        // require them to be successful)
        EXPECT_TRUE(j == json::parse(serializer.serialize(t)))
            << "Failed to serialize:\n" << t;
        EXPECT_TRUE(t == serializer.deserialize<T>(asBytes(j.dump())))
            << "Failed to deserialize:\n" << j;
    }

    template<typename T>
    void testFailedDeserializationHelper(const json& j)
    {
        EXPECT_THROW(
            serializer.deserialize<T>(asBytes(j.dump())),
            SerializationFailureException);
    }

private:
    JsonSerializer serializer;
};

TEST_F(JsonSerializerTest, testGeneral)
{
    const auto message = std::string {"hello"};
    testHelper(message, message);
}

TEST_F(JsonSerializerTest, testBid)
{
    const auto j = json {
        {BID_LEVEL_KEY, json(4)},
        {BID_STRAIN_KEY, Strains::HEARTS},
    };
    testHelper(BID, j);
}

TEST_F(JsonSerializerTest, testBidMissingLevel)
{
    const auto j = json {
        {BID_STRAIN_KEY, Strains::HEARTS},
    };
    testFailedDeserializationHelper<Bid>(j);
}

TEST_F(JsonSerializerTest, testBidLevelNotInteger)
{
    const auto j = json {
        {BID_LEVEL_KEY, json {}},
        {BID_STRAIN_KEY, Strains::HEARTS},
    };
    testFailedDeserializationHelper<Bid>(j);
}

TEST_F(JsonSerializerTest, testBidLevelInvalid)
{
    const auto j = json {
        {BID_LEVEL_KEY, Bid::MAXIMUM_LEVEL + 1},
        {BID_STRAIN_KEY, Strains::HEARTS},
    };
    testFailedDeserializationHelper<Bid>(j);
}

TEST_F(JsonSerializerTest, testBidMissingStrain)
{
    const auto j = json {{BID_LEVEL_KEY, 4}};
    testFailedDeserializationHelper<Bid>(j);
}

TEST_F(JsonSerializerTest, testBidStrainNotString)
{
    const auto j = json {{BID_LEVEL_KEY, json(4)}, {BID_STRAIN_KEY, json {}}};
    testFailedDeserializationHelper<Bid>(j);
}

TEST_F(JsonSerializerTest, testBidStrainInvalid)
{
    const auto j = json {
        {BID_LEVEL_KEY, json(4)}, {BID_STRAIN_KEY, json("invalid")}};
    testFailedDeserializationHelper<Bid>(j);
}

TEST_F(JsonSerializerTest, testCallPass)
{
    const auto j = json {{CALL_TYPE_KEY, CALL_PASS_TAG}};
    const auto call = Call {Pass {}};
    testHelper(call, j);
}

TEST_F(JsonSerializerTest, testCallBid)
{
    const auto j = json {
        {CALL_TYPE_KEY, CALL_BID_TAG},
        {CALL_BID_TAG, BID}
    };
    const auto call = Call {BID};
    testHelper(call, j);
}

TEST_F(JsonSerializerTest, testCallDouble)
{
    const auto j = json {{CALL_TYPE_KEY, CALL_DOUBLE_TAG}};
    const auto call = Call {Double {}};
    testHelper(call, j);
}

TEST_F(JsonSerializerTest, testCallRedouble)
{
    const auto j = json {{CALL_TYPE_KEY, CALL_REDOUBLE_TAG}};
    const auto call = Call {Redouble {}};
    testHelper(call, j);
}

TEST_F(JsonSerializerTest, testCallCallMissing)
{
    const auto j = json::object();
    testFailedDeserializationHelper<Call>(j);
}

TEST_F(JsonSerializerTest, testCardType)
{
    const auto j = json {
        {CARD_TYPE_RANK_KEY, Ranks::ACE},
        {CARD_TYPE_SUIT_KEY, Suits::SPADES}
    };
    const auto card_type = CardType {Ranks::ACE, Suits::SPADES};
    testHelper(card_type, j);
}

TEST_F(JsonSerializerTest, testCardTypeRankMissing)
{
    const auto j = json {
        {CARD_TYPE_RANK_KEY, Ranks::ACE}
    };
    testFailedDeserializationHelper<CardType>(j);
}

TEST_F(JsonSerializerTest, testCardTypeRankInvalid)
{
    const auto j = json {
        {CARD_TYPE_RANK_KEY, "invalid"},
        {CARD_TYPE_SUIT_KEY, Suits::SPADES}
    };
    testFailedDeserializationHelper<CardType>(j);
}

TEST_F(JsonSerializerTest, testCardTypeSuitMissing)
{
    const auto j = json {
        {CARD_TYPE_SUIT_KEY, Suits::SPADES}
    };
    testFailedDeserializationHelper<CardType>(j);
}

TEST_F(JsonSerializerTest, testCardTypeSuitInvalid)
{
    const auto j = json {
        {CARD_TYPE_RANK_KEY, Ranks::ACE},
        {CARD_TYPE_SUIT_KEY, "invalid"}
    };
    testFailedDeserializationHelper<CardType>(j);
}

TEST_F(JsonSerializerTest, testVulnerability)
{
    const auto j = json {
        {Partnerships::NORTH_SOUTH_VALUE, true},
        {Partnerships::EAST_WEST_VALUE, false}
    };
    testHelper(VULNERABILITY, j);
}

TEST_F(JsonSerializerTest, testVulnerabilityNorthSouthMissing)
{
    const auto j = json {
        {Partnerships::NORTH_SOUTH_VALUE, true}
    };
    testFailedDeserializationHelper<Vulnerability>(j);
}

TEST_F(JsonSerializerTest, testVulnerabilityNorthSouthInvalid)
{
    const auto j = json {
        {Partnerships::NORTH_SOUTH_VALUE, nullptr},
        {Partnerships::EAST_WEST_VALUE, false}
    };
    testFailedDeserializationHelper<Vulnerability>(j);
}

TEST_F(JsonSerializerTest, testVulnerabilityEastWestMissing)
{
    const auto j = json {
        {Partnerships::EAST_WEST_VALUE, false}
    };
    testFailedDeserializationHelper<Vulnerability>(j);
}

TEST_F(JsonSerializerTest, testVulnerabilityEastWestInvalid)
{
    const auto j = json {
        {Partnerships::NORTH_SOUTH_VALUE, true},
        {Partnerships::EAST_WEST_VALUE, nullptr}
    };
    testFailedDeserializationHelper<Vulnerability>(j);
}

TEST_F(JsonSerializerTest, testContract)
{
    const auto j = json {
        {CONTRACT_BID_KEY, BID},
        {CONTRACT_DOUBLING_KEY, Doubling::DOUBLED}};
    testHelper(CONTRACT, j);
}

TEST_F(JsonSerializerTest, testContractMissingBid)
{
    const auto j = json {
        {CONTRACT_DOUBLING_KEY, Doubling::DOUBLED}};
    testFailedDeserializationHelper<Contract>(j);
}

TEST_F(JsonSerializerTest, testContractInvalidBid)
{
    const auto j = json {
        {CONTRACT_BID_KEY, nullptr},
        {CONTRACT_DOUBLING_KEY, Doubling::DOUBLED}
    };
    testFailedDeserializationHelper<Contract>(j);
}

TEST_F(JsonSerializerTest, testContractMissingDoubling)
{
    const auto j = json {
        {CONTRACT_BID_KEY, BID}};
    testFailedDeserializationHelper<Contract>(j);
}

TEST_F(JsonSerializerTest, testContractInvalidDoubling)
{
    const auto j = json {
        {CONTRACT_BID_KEY, BID},
        {CONTRACT_DOUBLING_KEY, nullptr}
    };
    testFailedDeserializationHelper<Contract>(j);
}

TEST_F(JsonSerializerTest, testTricksWon)
{
    const auto j = json {
        {Partnerships::NORTH_SOUTH_VALUE, 5},
        {Partnerships::EAST_WEST_VALUE, 6}
    };
    testHelper(TRICKS_WON, j);
}

TEST_F(JsonSerializerTest, testTricksWonNorthSouthMissing)
{
    const auto j = json {
        {Partnerships::NORTH_SOUTH_VALUE, 5}
    };
    testFailedDeserializationHelper<TricksWon>(j);
}

TEST_F(JsonSerializerTest, testTricksWonNorthSouthInvalid)
{
    const auto j = json {
        {Partnerships::NORTH_SOUTH_VALUE, nullptr},
        {Partnerships::EAST_WEST_VALUE, 6}
    };
    testFailedDeserializationHelper<TricksWon>(j);
}

TEST_F(JsonSerializerTest, testTricksWonEastWestMissing)
{
    const auto j = json {
        {Partnerships::EAST_WEST_VALUE, 6}
    };
    testFailedDeserializationHelper<TricksWon>(j);
}

TEST_F(JsonSerializerTest, testTricksWonEastWestInvalid)
{
    const auto j = json {
        {Partnerships::NORTH_SOUTH_VALUE, 5},
        {Partnerships::EAST_WEST_VALUE, nullptr}
    };
    testFailedDeserializationHelper<TricksWon>(j);
}

TEST_F(JsonSerializerTest, testPeerEntry)
{
    const auto j = json {
        {
            CardServer::ENDPOINT_KEY,
            PEER_ENDPOINT
        },
        {
            CardServer::SERVER_KEY_KEY,
            PEER_SERVER_KEY
        }
    };
    const auto peerEntry = CardServer::PeerEntry {
        PEER_ENDPOINT, PEER_SERVER_KEY};
    testHelper(peerEntry, j);
}

TEST_F(JsonSerializerTest, testPeerEntryEndpointMissing)
{
    const auto j = json::object();
    testFailedDeserializationHelper<CardServer::PeerEntry>(j);
}

TEST_F(JsonSerializerTest, testPeerEntryEndpointInvalid)
{
    const auto j = json {
        {
            CardServer::ENDPOINT_KEY,
            123
        }
    };
    testFailedDeserializationHelper<CardServer::PeerEntry>(j);
}

TEST_F(JsonSerializerTest, testPeerEntryServerKeyMissing)
{
    const auto j = json {
        {
            CardServer::ENDPOINT_KEY,
            PEER_ENDPOINT
        },
    };
    const auto peerEntry = CardServer::PeerEntry {PEER_ENDPOINT};
    testHelper(peerEntry, j);
}

TEST_F(JsonSerializerTest, testPeerEntryServerKeyInvalid)
{
    const auto j = json {
        {
            CardServer::ENDPOINT_KEY,
            PEER_ENDPOINT
        },
        {
            CardServer::SERVER_KEY_KEY,
            nullptr
        },
    };
    testFailedDeserializationHelper<CardServer::PeerEntry>(j);
}

TEST_F(JsonSerializerTest, testUuid)
{
    const auto uuid_string = "a3cc5805-544f-415b-ba86-31f6237bf122";
    boost::uuids::string_generator gen;
    const auto uuid = gen(uuid_string);
    const auto j = json(uuid_string);
    testHelper(uuid, j);
}

TEST_F(JsonSerializerTest, testUuidInvalidType)
{
    const auto j = json(5);
    testFailedDeserializationHelper<Uuid>(j);
}

TEST_F(JsonSerializerTest, testUuidInvalidFormat)
{
    const auto j = json("invalid");
    testFailedDeserializationHelper<Uuid>(j);
}

TEST_F(JsonSerializerTest, testDuplicateScore)
{
    const auto j = json {
        {
            Scoring::DUPLICATE_SCORE_PARTNERSHIP_KEY,
            Partnerships::NORTH_SOUTH
        },
        {
            Scoring::DUPLICATE_SCORE_SCORE_KEY,
            100
        }
    };
    const auto score = DuplicateScore {Partnerships::NORTH_SOUTH, 100};
    testHelper(score, j);
}

TEST_F(JsonSerializerTest, testDuplicateScorePartnershipMissing)
{
    const auto j = json {
        {Scoring::DUPLICATE_SCORE_SCORE_KEY, 100}};
    testFailedDeserializationHelper<DuplicateScore>(j);
}

TEST_F(JsonSerializerTest, testDuplicateScorePartnershipInvalid)
{
    const auto j = json {
        {Scoring::DUPLICATE_SCORE_PARTNERSHIP_KEY, "invalid"},
        {Scoring::DUPLICATE_SCORE_SCORE_KEY, 100}};
    testFailedDeserializationHelper<DuplicateScore>(j);
}

TEST_F(JsonSerializerTest, testDuplicateScoreScoreMissing)
{
    const auto j = json {
        {
            Scoring::DUPLICATE_SCORE_PARTNERSHIP_KEY,
            Partnerships::NORTH_SOUTH
        }
    };
    testFailedDeserializationHelper<DuplicateScore>(j);
}

TEST_F(JsonSerializerTest, testDuplicateScoreScoreInvalid)
{
    const auto j = json {
        {
            Scoring::DUPLICATE_SCORE_PARTNERSHIP_KEY,
            Partnerships::NORTH_SOUTH
        },
        {Scoring::DUPLICATE_SCORE_SCORE_KEY, "invalid"}
    };
    testFailedDeserializationHelper<DuplicateScore>(j);
}
