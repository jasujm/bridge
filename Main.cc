// TODO: This front-end is far from finished...

#include "bridge/BasicPlayer.hh"
#include "bridge/Call.hh"
#include "bridge/CardType.hh"
#include "bridge/DealResult.hh"
#include "bridge/GameState.hh"
#include "bridge/Position.hh"
#include "bridge/Vulnerability.hh"
#include "engine/SimpleCardManager.hh"
#include "engine/DuplicateGameManager.hh"
#include "main/BridgeMain.hh"
#include "main/CommandInterpreter.hh"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

template<typename CardTypeIterator>
void printSuit(Bridge::Suit suit, CardTypeIterator first, CardTypeIterator last)
{
    std::cout << suit << ": ";
    std::sort(first, last,
              [](const auto& lhs, const auto& rhs) {
                  return lhs.rank < rhs.rank;
              });
    for (; first != last; ++first) {
        std::cout << first->rank << " ";
    }
    std::cout << "\n";
}

void printCards(
    const std::map<Bridge::Position, std::vector<Bridge::CardType>>& cards)
{
    for (const auto& pair : cards) {
        std::cout << pair.first << "\n";
        auto player_cards = pair.second;
        auto suit_begin = player_cards.begin();
        for (const auto suit : Bridge::SUITS) {
            auto suit_end = std::partition(
                suit_begin, player_cards.end(),
                [=](const auto& type)
                {
                    return type.suit == suit;
                });
            printSuit(suit, suit_begin, suit_end);
        }
    }
}

void printCalls(
    Bridge::Vulnerability vulnerability,
    const std::vector<std::pair<Bridge::Position, Bridge::Call>>& calls)
{
    for (const auto position : Bridge::POSITIONS) {
        std::cout << position;
        if (isVulnerable(vulnerability, partnershipFor(position))) {
            std::cout << " V";
        }
        std::cout << "\t";
    }
    std::cout << "\n";
    auto iter = calls.begin();
    while (iter != calls.end()) {
        for (const auto position : Bridge::POSITIONS)  {
            if (iter != calls.end() && iter->first == position) {
                std::cout << iter->second;
                ++iter;
            }
            std::cout << "\t";
        }
        std::cout << "\n";
    }
}

void printTrick(const std::map<Bridge::Position, Bridge::CardType>& trick)
{
    for (const auto position : Bridge::POSITIONS) {
        std::cout << position << ": ";
        const auto iter = trick.find(position);
        if (iter != trick.end()) {
            std::cout << iter->second;
        }
        std::cout << "\n";
    }
}

void printTricksWon(const Bridge::DealResult& result)
{
    std::cout << "North-South: " << result.tricksWonByNorthSouth << "\n"
              << "East-West: " << result.tricksWonByEastWest << "\n";
}

void printGameState(const Bridge::GameState& gameState)
{
    if (const auto& cards = gameState.cards) {
        std::cout << "\nCards:\n";
        printCards(*cards);
    }
    if (const auto& calls = gameState.calls) {
        std::cout << "\nCalls:\n";
        printCalls(
            gameState.vulnerability.value_or(Bridge::Vulnerability::NEITHER),
            *calls);
    }
    if (const auto& result = gameState.playingResult) {
        std::cout << "\nCurrent trick:\n";
        printTrick(result->currentTrick);
        std::cout << "\nTricks won:\n";
        printTricksWon(result->dealResult);
    }
}

void printScore(const Bridge::Engine::DuplicateGameManager& gameManager)
{
    std::cout << "\nScore:\n";
    std::cout << "NS\tEW\n";
    for (const auto& entry : getScoreEntries(gameManager)) {
        if (entry) {
            if (entry->partnership == Bridge::Partnership::EAST_WEST) {
                std::cout << "\t";
            }
            std::cout << entry->score << "\n";
        } else {
            std::cout << "-\t-\n";
        }
    }
}

int main()
{
    using namespace Bridge;

    auto card_manager = std::make_shared<Engine::SimpleCardManager>();
    auto game_manager = std::make_shared<Engine::DuplicateGameManager>();
    const auto players {
        std::make_shared<BasicPlayer>(),
        std::make_shared<BasicPlayer>(),
        std::make_shared<BasicPlayer>(),
        std::make_shared<BasicPlayer>()};
    auto bridge_main = Main::BridgeMain::makeBridgeMain(
        card_manager, game_manager, players.begin(), players.end());
    Main::CommandInterpreter interpreter {*bridge_main};

    for (bool go = true; go && std::cin.good(); ) {
        const auto game_state = bridge_main->getState();
        printGameState(game_state);
        if (!game_manager->hasEnded()) {
            if (const auto position_in_turn = game_state.positionInTurn) {
                std::cout << "\nCommand for " << *position_in_turn << ": ";
            }
            auto command = std::string {};
            std::getline(std::cin, command);
            if (command == "score") {
                printScore(*game_manager);
            } else if (command == "quit") {
                go = false;
            } else {
                interpreter.interpret(command);
            }
        } else {
            go = false;
        }
    }

    return EXIT_SUCCESS;
}
