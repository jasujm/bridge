#include "bridge/Bid.hh"
#include "bridge/CardType.hh"
#include "bridge/Call.hh"
#include "bridge/Contract.hh"
#include "bridge/DealState.hh"
#include "bridge/Partnership.hh"
#include "bridge/TricksWon.hh"
#include "bridge/Vulnerability.hh"
#include "cardserver/PeerEntry.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/BidJsonSerializer.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/ContractJsonSerializer.hh"
#include "messaging/DealStateJsonSerializer.hh"
#include "messaging/DuplicateScoreSheetJsonSerializer.hh"
#include "messaging/PartnershipJsonSerializer.hh"
#include "messaging/PeerEntryJsonSerializer.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "messaging/SerializationFailureException.hh"
#include "messaging/TricksWonJsonSerializer.hh"
#include "messaging/VulnerabilityJsonSerializer.hh"
#include "scoring/DuplicateScoreSheet.hh"

#include <json.hpp>
#include <gtest/gtest.h>

using namespace Bridge;
using namespace Bridge::Messaging;
using namespace Bridge::Scoring;

using nlohmann::json;

using namespace std::string_literals;

namespace {
const auto BID = Bid {4, Strain::HEARTS};
const auto CONTRACT = Contract {BID, Doubling::DOUBLED};
const auto VULNERABILITY = Vulnerability {true, false};
const auto TRICKS_WON = TricksWon {5, 6};
const auto PEER_IDENTITY = "peer"s;
const auto PEER_ENDPOINT = "inproc://test"s;
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
        EXPECT_TRUE(t == serializer.deserialize<T>(j.dump()))
            << "Failed to deserialize:\n" << j;
    }

    template<typename T>
    void testFailedDeserializationHelper(const json& j)
    {
        EXPECT_THROW(
            serializer.deserialize<T>(j.dump()),
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
        {BID_STRAIN_KEY, json(STRAIN_TO_STRING_MAP.left.at(Strain::HEARTS))}};
    testHelper(BID, j);
}

TEST_F(JsonSerializerTest, testBidMissingLevel)
{
    const auto j = json {
        {BID_STRAIN_KEY, STRAIN_TO_STRING_MAP.left.at(Strain::HEARTS)}};
    testFailedDeserializationHelper<Bid>(j);
}

TEST_F(JsonSerializerTest, testBidLevelNotInteger)
{
    const auto j = json {
        {BID_LEVEL_KEY, json {}},
        {BID_STRAIN_KEY, json(STRAIN_TO_STRING_MAP.left.at(Strain::HEARTS))}};
    testFailedDeserializationHelper<Bid>(j);
}

TEST_F(JsonSerializerTest, testBidLevelInvalid)
{
    const auto j = json {
        {BID_LEVEL_KEY, Bid::MAXIMUM_LEVEL + 1},
        {BID_STRAIN_KEY, json(STRAIN_TO_STRING_MAP.left.at(Strain::HEARTS))}};
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
        {CALL_TYPE_KEY, json(CALL_BID_TAG)}, {CALL_BID_TAG, toJson(BID)}};
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
        {CARD_TYPE_RANK_KEY, RANK_TO_STRING_MAP.left.at(Rank::ACE)},
        {CARD_TYPE_SUIT_KEY, SUIT_TO_STRING_MAP.left.at(Suit::SPADES)}};
    const auto card_type = CardType {Rank::ACE, Suit::SPADES};
    testHelper(card_type, j);
}

TEST_F(JsonSerializerTest, testCardTypeRankMissing)
{
    const auto j = json {
        {CARD_TYPE_RANK_KEY, RANK_TO_STRING_MAP.left.at(Rank::ACE)}};
    testFailedDeserializationHelper<CardType>(j);
}

TEST_F(JsonSerializerTest, testCardTypeRankInvalid)
{
    const auto j = json {
        {CARD_TYPE_RANK_KEY, "invalid"},
        {CARD_TYPE_SUIT_KEY, SUIT_TO_STRING_MAP.left.at(Suit::SPADES)}};
    testFailedDeserializationHelper<CardType>(j);
}

TEST_F(JsonSerializerTest, testCardTypeSuitMissing)
{
    const auto j = json {
        {CARD_TYPE_SUIT_KEY, SUIT_TO_STRING_MAP.left.at(Suit::SPADES)}};
    testFailedDeserializationHelper<CardType>(j);
}

TEST_F(JsonSerializerTest, testCardTypeSuitInvalid)
{
    const auto j = json {
        {CARD_TYPE_RANK_KEY, RANK_TO_STRING_MAP.left.at(Rank::ACE)},
        {CARD_TYPE_SUIT_KEY, "invalid"}};
    testFailedDeserializationHelper<CardType>(j);
}

TEST_F(JsonSerializerTest, testVulnerability)
{
    const auto j = json {
        {PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::NORTH_SOUTH), true},
        {PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::EAST_WEST), false}};
    testHelper(VULNERABILITY, j);
}

TEST_F(JsonSerializerTest, testVulnerabilityNorthSouthMissing)
{
    const auto j = json {
        {PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::NORTH_SOUTH), true}};
    testFailedDeserializationHelper<Vulnerability>(j);
}

TEST_F(JsonSerializerTest, testVulnerabilityNorthSouthInvalid)
{
    const auto j = json {
        {PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::NORTH_SOUTH), nullptr},
        {PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::EAST_WEST), false}};
    testFailedDeserializationHelper<Vulnerability>(j);
}

TEST_F(JsonSerializerTest, testVulnerabilityEastWestMissing)
{
    const auto j = json {
        {PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::EAST_WEST), false}};
    testFailedDeserializationHelper<Vulnerability>(j);
}

TEST_F(JsonSerializerTest, testVulnerabilityEastWestInvalid)
{
    const auto j = json {
        {PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::NORTH_SOUTH), true},
        {PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::EAST_WEST), nullptr}};
    testFailedDeserializationHelper<Vulnerability>(j);
}

TEST_F(JsonSerializerTest, testContract)
{
    const auto j = json {
        {CONTRACT_BID_KEY, toJson(BID)},
        {CONTRACT_DOUBLING_KEY, toJson(Doubling::DOUBLED)}};
    testHelper(CONTRACT, j);
}

TEST_F(JsonSerializerTest, testContractMissingBid)
{
    const auto j = json {
        {CONTRACT_DOUBLING_KEY, toJson(Doubling::DOUBLED)}};
    testFailedDeserializationHelper<Contract>(j);
}

TEST_F(JsonSerializerTest, testContractInvalidBid)
{
    const auto j = json {
        {CONTRACT_BID_KEY, nullptr},
        {CONTRACT_DOUBLING_KEY, toJson(Doubling::DOUBLED)}};
    testFailedDeserializationHelper<Contract>(j);
}

TEST_F(JsonSerializerTest, testContractMissingDoubling)
{
    const auto j = json {
        {CONTRACT_BID_KEY, toJson(BID)}};
    testFailedDeserializationHelper<Contract>(j);
}

TEST_F(JsonSerializerTest, testContractInvalidDoubling)
{
    const auto j = json {
        {CONTRACT_BID_KEY, toJson(BID)},
        {CONTRACT_DOUBLING_KEY, nullptr}};
    testFailedDeserializationHelper<Contract>(j);
}

TEST_F(JsonSerializerTest, testTricksWon)
{
    const auto j = json {
        {PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::NORTH_SOUTH), 5},
        {PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::EAST_WEST), 6}};
    testHelper(TRICKS_WON, j);
}

TEST_F(JsonSerializerTest, testTricksWonNorthSouthMissing)
{
    const auto j = json {
        {PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::NORTH_SOUTH), 5}};
    testFailedDeserializationHelper<TricksWon>(j);
}

TEST_F(JsonSerializerTest, testTricksWonNorthSouthInvalid)
{
    const auto j = json {
        {PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::NORTH_SOUTH), nullptr},
        {PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::EAST_WEST), 6}};
    testFailedDeserializationHelper<TricksWon>(j);
}

TEST_F(JsonSerializerTest, testTricksWonEastWestMissing)
{
    const auto j = json {
        {PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::EAST_WEST), 6}};
    testFailedDeserializationHelper<TricksWon>(j);
}

TEST_F(JsonSerializerTest, testTricksWonEastWestInvalid)
{
    const auto j = json {
        {PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::NORTH_SOUTH), 5},
        {PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::EAST_WEST), nullptr}};
    testFailedDeserializationHelper<TricksWon>(j);
}

TEST_F(JsonSerializerTest, testSimpleDealState)
{
    const auto j = json {
        {DEAL_STATE_STAGE_KEY, toJson(Stage::SHUFFLING)}};
    auto state = DealState {};
    state.stage = Stage::SHUFFLING;
    testHelper(state, j);
}

TEST_F(JsonSerializerTest, testComplexDealState)
{
    const auto card1 = CardType {Rank::ACE, Suit::SPADES};
    const auto card2 = CardType {Rank::TWO, Suit::CLUBS};
    const auto card3 = CardType {Rank::SEVEN, Suit::DIAMONDS};
    const auto cards = DealState::Cards {
        { Position::NORTH, {card1} },
        { Position::SOUTH, {card2, card3} }
    };
    const auto call1 = Call {BID};
    const auto call2 = Call {Double {}};
    const auto calls = DealState::Calls {
        { Position::SOUTH, call1 },
        { Position::WEST, call2 }
    };
    const auto trick = DealState::Trick {
        { Position::EAST, card1 },
        { Position::WEST, card2 }
    };
    const auto j = json {
        {
            DEAL_STATE_STAGE_KEY,
            toJson(Stage::PLAYING)
        },
        {
            DEAL_STATE_POSITION_IN_TURN_KEY,
            toJson(Position::EAST)
        },
        {
            DEAL_STATE_VULNERABILITY_KEY,
            toJson(VULNERABILITY)
        },
        {
            DEAL_STATE_CARDS_KEY,
            {
                {
                    POSITION_TO_STRING_MAP.left.at(Position::NORTH),
                    {
                        toJson(card1)
                    }
                },
                {
                    POSITION_TO_STRING_MAP.left.at(Position::SOUTH),
                    {
                        toJson(card2),
                        toJson(card3)
                    }
                }
            }
        },
        {
            DEAL_STATE_CALLS_KEY,
            {
                {
                    {
                        DEAL_STATE_POSITION_KEY,
                        toJson(Position::SOUTH)
                    },
                    {
                        DEAL_STATE_CALL_KEY,
                        toJson(call1)
                    },
                },
                {
                    {
                        DEAL_STATE_POSITION_KEY,
                        toJson(Position::WEST)
                    },
                    {
                        DEAL_STATE_CALL_KEY,
                        toJson(call2)
                    },
                }
            }
        },
        {
            DEAL_STATE_DECLARER_KEY,
            toJson(Position::SOUTH)
        },
        {
            DEAL_STATE_CONTRACT_KEY,
            toJson(CONTRACT)
        },
        {
            DEAL_STATE_CURRENT_TRICK_KEY,
            {
                {
                    {
                        DEAL_STATE_POSITION_KEY,
                        toJson(Position::EAST)
                    },
                    {
                        DEAL_STATE_CARD_KEY,
                        toJson(card1)
                    }
                },
                {
                    {
                        DEAL_STATE_POSITION_KEY,
                        toJson(Position::WEST)
                    },
                    {
                        DEAL_STATE_CARD_KEY,
                        toJson(card2)
                    }
                }
            }
        },
        {
            DEAL_STATE_TRICKS_WON_KEY,
            toJson(TRICKS_WON)
        }
    };
    auto state = DealState {};
    state.stage = Stage::PLAYING;
    state.positionInTurn = Position::EAST;
    state.vulnerability = VULNERABILITY;
    state.cards = cards;
    state.calls = calls;
    state.declarer = Position::SOUTH;
    state.contract = CONTRACT;
    state.currentTrick = trick;
    state.tricksWon = TRICKS_WON;
    testHelper(state, j);
}

TEST_F(JsonSerializerTest, testDuplicateScoreSheet)
{
    const auto j = json {
        nullptr,
        {
            {
                DUPLICATE_SCORE_SHEET_PARTNERSHIP_KEY,
                toJson(Partnership::NORTH_SOUTH)
            },
            {
                DUPLICATE_SCORE_SHEET_SCORE_KEY,
                50
            }
        },
        {
            {
                DUPLICATE_SCORE_SHEET_PARTNERSHIP_KEY,
                toJson(Partnership::EAST_WEST)
            },
            {
                DUPLICATE_SCORE_SHEET_SCORE_KEY,
                100
            }
        },
    };
    const auto scoreSheet = DuplicateScoreSheet {
        boost::none,
        DuplicateScoreSheet::Score {Partnership::NORTH_SOUTH, 50},
        DuplicateScoreSheet::Score {Partnership::EAST_WEST, 100}
    };
    testHelper(scoreSheet, j);
}

TEST_F(JsonSerializerTest, testScoreSheetPartnershipMissing)
{
    const auto j = json {
        {
            {
                DUPLICATE_SCORE_SHEET_SCORE_KEY,
                50
            }
        }
    };
    testFailedDeserializationHelper<DuplicateScoreSheet>(j);
}

TEST_F(JsonSerializerTest, testDuplicateScoreSheetPartnershipInvalid)
{
    const auto j = json {
        {
            {
                DUPLICATE_SCORE_SHEET_PARTNERSHIP_KEY,
                nullptr
            },
            {
                DUPLICATE_SCORE_SHEET_SCORE_KEY,
                50
            }
        }
    };
    testFailedDeserializationHelper<DuplicateScoreSheet>(j);
}

TEST_F(JsonSerializerTest, testScoreSheetScoreMissing)
{
    const auto j = json {
        {
            {
                DUPLICATE_SCORE_SHEET_PARTNERSHIP_KEY,
                toJson(Partnership::NORTH_SOUTH)
            }
        }
    };
    testFailedDeserializationHelper<DuplicateScoreSheet>(j);
}

TEST_F(JsonSerializerTest, testDuplicateScoreSheetScoreInvalid)
{
    const auto j = json {
        {
            {
                DUPLICATE_SCORE_SHEET_PARTNERSHIP_KEY,
                toJson(Partnership::NORTH_SOUTH)
            },
            {
                DUPLICATE_SCORE_SHEET_SCORE_KEY,
                nullptr
            }
        }
    };
    testFailedDeserializationHelper<DuplicateScoreSheet>(j);
}

TEST_F(JsonSerializerTest, testPeerEntry)
{
    const auto j = json {
        {
            IDENTITY_KEY,
            PEER_IDENTITY
        },
        {
            ENDPOINT_KEY,
            PEER_ENDPOINT
        }
    };
    const auto peerEntry = CardServer::PeerEntry {PEER_IDENTITY, PEER_ENDPOINT};
    testHelper(peerEntry, j);
}

TEST_F(JsonSerializerTest, testPeerEntryIdentityMissing)
{
    const auto j = json {
        {
            ENDPOINT_KEY,
            PEER_ENDPOINT
        }
    };
    testFailedDeserializationHelper<CardServer::PeerEntry>(j);
}

TEST_F(JsonSerializerTest, testPeerEntryIdentityInvalid)
{
    const auto j = json {
        {
            IDENTITY_KEY,
            nullptr
        },
        {
            ENDPOINT_KEY,
            PEER_ENDPOINT
        }
    };
    testFailedDeserializationHelper<CardServer::PeerEntry>(j);
}

TEST_F(JsonSerializerTest, testPeerEntryEndpointMissing)
{
    const auto j = json {
        {
            IDENTITY_KEY,
            PEER_IDENTITY
        },
    };
    const auto peerEntry = CardServer::PeerEntry {PEER_IDENTITY};
    testHelper(peerEntry, j);
}

TEST_F(JsonSerializerTest, testPeerEntryEndpointInvalid)
{
    const auto j = json {
        {
            IDENTITY_KEY,
            PEER_IDENTITY
        },
        {
            ENDPOINT_KEY,
            123
        }
    };
    testFailedDeserializationHelper<CardServer::PeerEntry>(j);
}
