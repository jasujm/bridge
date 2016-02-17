// TODO: This front-end is far from finished...

#include "bridge/Call.hh"
#include "bridge/CardType.hh"
#include "bridge/DealState.hh"
#include "bridge/Position.hh"
#include "bridge/TricksWon.hh"
#include "bridge/Vulnerability.hh"
#include "main/BridgeMain.hh"
#include "main/BridgeController.hh"
#include "main/CommandInterpreter.hh"
#include "messaging/CommandUtility.hh"
#include "messaging/DealStateJsonSerializer.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/DuplicateScoreSheetJsonSerializer.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/MessageUtility.hh"
#include "scoring/DuplicateScoreSheet.hh"

#include <zmq.hpp>

#include <algorithm>
#include <iostream>
#include <vector>

using namespace Bridge;
using namespace Bridge::Main;
using namespace Bridge::Messaging;
using namespace Bridge::Scoring;

void printCards(std::vector<CardType> cards)
{
    std::sort(
        cards.begin(), cards.end(),
        [](const auto& type1, const auto& type2)
        {
            return std::make_pair(type1.suit, type1.rank) <
                std::make_pair(type2.suit, type2.rank);
        });
    auto suit_begin = cards.begin();
    for (const auto suit : SUITS) {
        const auto suit_end = std::find_if(
            suit_begin, cards.end(),
            [suit](const auto& type)
            {
                return type.suit != suit;
            });
        std::cout << suit << ":";
        for (; suit_begin != suit_end; ++suit_begin) {
            std::cout << " " << suit_begin->rank;
        }
        std::cout << "\n";
    }
}

void printAllCards(const DealState::Cards& all_cards)
{
    for (const auto& cards : all_cards) {
        std::cout << cards.first << "\n";
        printCards(cards.second);
    }
}

void printCalls(Vulnerability vulnerability, const DealState::Calls& calls)
{
    for (const auto position : POSITIONS) {
        std::cout << position;
        if (isVulnerable(vulnerability, partnershipFor(position))) {
            std::cout << " V";
        }
        std::cout << "\t";
    }
    std::cout << "\n";
    auto iter = calls.begin();
    while (iter != calls.end()) {
        for (const auto position : POSITIONS)  {
            if (iter != calls.end() && iter->first == position) {
                std::cout << iter->second;
                ++iter;
            }
            std::cout << "\t";
        }
        std::cout << "\n";
    }
}

void printTrick(const DealState::Trick& trick)
{
    for (const auto pair : trick) {
        std::cout << pair.first << ": " <<  pair.second << "\n";
    }
}

void printTricksWon(const TricksWon& result)
{
    std::cout << "North-South: " << result.tricksWonByNorthSouth << "\n"
              << "East-West: " << result.tricksWonByEastWest << "\n";
}

void printDealState(const DealState& gameState)
{
    if (const auto& cards = gameState.cards) {
        std::cout << "\nCards:\n";
        printAllCards(*cards);
    }
    if (const auto& calls = gameState.calls) {
        std::cout << "\nCalls:\n";
        printCalls(
            gameState.vulnerability.value_or(
                Vulnerability {false, false}), *calls);
    }
    if (const auto& current_trick = gameState.currentTrick) {
        std::cout << "\nCurrent trick:\n";
        printTrick(*current_trick);
    }
    if (const auto& tricks_won = gameState.tricksWon) {
        std::cout << "\nTricks won:\n";
        printTricksWon(*tricks_won);
    }
}

void printScore(const DuplicateScoreSheet& scoreSheet)
{
    std::cout << "\nScore:\n";
    std::cout << "NS\tEW\n";
    for (const auto& entry : scoreSheet) {
        if (entry) {
            if (entry->partnership == Partnership::EAST_WEST) {
                std::cout << "\t";
            }
            std::cout << entry->score << "\n";
        } else {
            std::cout << "-\t-\n";
        }
    }
}

void checkReplySuccessful(
    zmq::socket_t& socket,
    const std::string& expectedReply = MessageQueue::REPLY_SUCCESS,
    bool more = false)
{
    const auto reply = recvMessage(socket);
    if (reply != std::make_pair(expectedReply, more))
    {
        std::cerr << "Unknown failure" << std::endl;
        std::exit(1);
    }
}

std::string checkReply(zmq::socket_t& socket, bool more = false)
{
    const auto reply = recvMessage(socket);
    if (more != reply.second)
    {
        std::cerr << "Unknown failure" << std::endl;
        std::exit(1);
    }
    return reply.first;
}

class RemoteBridgeController : public BridgeController {
public:
    RemoteBridgeController(zmq::socket_t& socket) : socket {socket}
    {
    }
private:
    void handleCall(const Call& call) override
    {
        sendCommand(socket, JsonSerializer {}, BridgeMain::CALL_COMMAND, call);
        checkReplySuccessful(socket);
    }

    void handlePlay(const CardType& cardType) override
    {
        sendCommand(
            socket, JsonSerializer {}, BridgeMain::PLAY_COMMAND, cardType);
        checkReplySuccessful(socket);
    }

    zmq::socket_t& socket;
};

int main()
{
    const auto controlEndpoint = std::string {"inproc://bridgemain"};
    const auto dataEndpoint = std::string {"inproc://bridgedata"};

    zmq::context_t context;
    BridgeMain bridge_main {context, controlEndpoint, dataEndpoint};

    zmq::socket_t controlSocket {context, zmq::socket_type::req};
    controlSocket.connect(controlEndpoint);

    zmq::socket_t dataSocket {context, zmq::socket_type::sub};
    dataSocket.setsockopt(
        ZMQ_SUBSCRIBE, BridgeMain::STATE_PREFIX.c_str(),
        BridgeMain::STATE_PREFIX.size());
    dataSocket.setsockopt(
        ZMQ_SUBSCRIBE, BridgeMain::SCORE_PREFIX.c_str(),
        BridgeMain::SCORE_PREFIX.size());
    dataSocket.connect(dataEndpoint);

    RemoteBridgeController controller {controlSocket};
    CommandInterpreter interpreter {controller};

    sendMessage(controlSocket, BridgeMain::STATE_COMMAND);
    checkReplySuccessful(controlSocket);

    auto go = true;
    DealState deal_state;
    while (go && std::cin.good()) {
        {
            checkReplySuccessful(dataSocket, BridgeMain::STATE_PREFIX, true);
            const auto reply = checkReply(dataSocket);
            deal_state = JsonSerializer::deserialize<DealState>(reply);
        }
        printDealState(deal_state);
        if (deal_state.stage != Stage::ENDED) {
            if (const auto position_in_turn = deal_state.positionInTurn) {
                std::cout << "\nCommand for " << *position_in_turn << ": ";
            }
            auto command = std::string {};
            std::getline(std::cin, command);
            if (command == "score") {
                sendMessage(controlSocket, BridgeMain::SCORE_COMMAND);
                checkReplySuccessful(controlSocket);
                checkReplySuccessful(
                    dataSocket, BridgeMain::SCORE_PREFIX, true);
                const auto reply = checkReply(dataSocket);
                const auto score_sheet =
                    JsonSerializer::deserialize<DuplicateScoreSheet>(reply);
                printScore(score_sheet);
                sendMessage(controlSocket, BridgeMain::STATE_COMMAND);
                checkReplySuccessful(controlSocket);
            } else if (command == "quit") {
                go = false;
            } else {
                const auto success = interpreter.interpret(command);
                if (!success) {
                    sendMessage(controlSocket, BridgeMain::STATE_COMMAND);
                    checkReplySuccessful(controlSocket);
                }
            }
        } else {
            go = false;
        }
    }

    sendMessage(controlSocket, MessageQueue::MESSAGE_TERMINATE);
    checkReplySuccessful(controlSocket);

    return EXIT_SUCCESS;
}
