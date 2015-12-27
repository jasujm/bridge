#include "main/CommandInterpreter.hh"

#include "bridge/BridgeGame.hh"
#include "bridge/Call.hh"
#include "bridge/CardType.hh"

#include <boost/optional/optional.hpp>

#include <sstream>

namespace Bridge {
namespace Main {

namespace {

boost::optional<Call> interpretCall(std::istringstream& is)
{
    auto arg = std::string {};
    is >> arg;
    if (arg == "pass") {
        return Call {Pass {}};
    } else if (arg == "double") {
        return Call {Double {}};
    } else if (arg == "redouble") {
        return Call {Redouble {}};
    } else if (arg == "bid") {
        int level;
        Strain strain;
        if (is >> level >> strain && Bid::levelValid(level)) {
            return Call {Bid {level, strain}};
        }
    }
    return boost::none;
}

boost::optional<CardType> interpretCardType(std::istringstream& is)
{
    Rank rank;
    Suit suit;
    if (is >> rank >> suit) {
        return CardType {rank, suit};
    }
    return boost::none;
}

}

CommandInterpreter::CommandInterpreter(BridgeGame& game) :
    game {game}
{
}

bool CommandInterpreter::interpret(const std::string& command)
{
    std::istringstream is {command};
    auto command_type = std::string {};
    is >> command_type;
    if (command_type == "call") {
        if (const auto call = interpretCall(is)) {
            game.call(*call);
            return true;
        }
    } else if (command_type == "play") {
        if (const auto card = interpretCardType(is)) {
            game.play(*card);
            return true;
        }
    }
    return false;
}

}
}
