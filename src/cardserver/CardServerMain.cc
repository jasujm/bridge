#include "cardserver/CardServerMain.hh"

namespace Bridge {
namespace CardServer {

class CardServerMain::Impl {
};

CardServerMain::CardServerMain(zmq::context_t&, const std::string&)
{
}

CardServerMain::~CardServerMain() = default;

void CardServerMain::run()
{
}

}
}
