// TODO: This front-end is far from finished...

#include "bridge/Call.hh"
#include "bridge/CardType.hh"
#include "bridge/DealResult.hh"
#include "bridge/GameState.hh"
#include "bridge/Position.hh"
#include "bridge/Vulnerability.hh"
#include "engine/DuplicateGameManager.hh"
#include "main/BridgeMain.hh"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

using namespace Bridge;

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

void printAllCards(const std::map<Position, std::vector<CardType>>& all_cards)
{
    for (const auto& cards : all_cards) {
        std::cout << cards.first << "\n";
        printCards(cards.second);
    }
}

void printCalls(
    Vulnerability vulnerability,
    const std::vector<std::pair<Position, Call>>& calls)
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

void printTrick(const std::map<Position, CardType>& trick)
{
    for (const auto position : POSITIONS) {
        std::cout << position << ": ";
        const auto iter = trick.find(position);
        if (iter != trick.end()) {
            std::cout << iter->second;
        }
        std::cout << "\n";
    }
}

void printTricksWon(const DealResult& result)
{
    std::cout << "North-South: " << result.tricksWonByNorthSouth << "\n"
              << "East-West: " << result.tricksWonByEastWest << "\n";
}

void printGameState(const GameState& gameState)
{
    if (const auto& cards = gameState.cards) {
        std::cout << "\nCards:\n";
        printAllCards(*cards);
    }
    if (const auto& calls = gameState.calls) {
        std::cout << "\nCalls:\n";
        printCalls(
            gameState.vulnerability.value_or(Vulnerability::NEITHER), *calls);
    }
    if (const auto& result = gameState.playingResult) {
        std::cout << "\nCurrent trick:\n";
        printTrick(result->currentTrick);
        std::cout << "\nTricks won:\n";
        printTricksWon(result->dealResult);
    }
}

void printScore(const Engine::DuplicateGameManager& gameManager)
{
    std::cout << "\nScore:\n";
    std::cout << "NS\tEW\n";
    for (const auto& entry : getScoreEntries(gameManager)) {
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

int main()
{
    Main::BridgeMain bridge_main;
    const auto& game_manager = bridge_main.getGameManager();

    auto go = true;
    while (go && std::cin.good()) {
        const auto game_state = bridge_main.getState();
        printGameState(game_state);
        if (!game_manager.hasEnded()) {
            if (const auto position_in_turn = game_state.positionInTurn) {
                std::cout << "\nCommand for " << *position_in_turn << ": ";
            }
            auto command = std::string {};
            std::getline(std::cin, command);
            if (command == "score") {
                printScore(game_manager);
            } else if (command == "quit") {
                go = false;
            } else {
                bridge_main.processCommand(command);
            }
        } else {
            go = false;
        }
    }

    return EXIT_SUCCESS;
}
