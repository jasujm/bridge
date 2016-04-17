#include "engine/SimpleCardManager.hh"

#include "bridge/BasicHand.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/SimpleCard.hh"
#include "Utility.hh"

#include <boost/statechart/event.hpp>
#include <boost/statechart/in_state_reaction.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/state.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/transition.hpp>

#include <cassert>

namespace sc = boost::statechart;

namespace Bridge {
namespace Engine {

namespace {

using CardVector = std::vector<SimpleCard>;

class Idle;
class Active;
class ShuffleRequested;
class ShuffleCompleted;

class RequestShuffleEvent : public sc::event<RequestShuffleEvent> {};
class NotifyStateEvent : public sc::event<NotifyStateEvent> {
public:
    explicit NotifyStateEvent(CardManager::ShufflingState state) :
        state {state}
    {
    }
    CardManager::ShufflingState state;
};

}

class SimpleCardManager::Impl :
        public sc::state_machine<Impl, Idle>,
        protected Observable<ShufflingState>  {
public:
    using CardTypeRange = SimpleCardManager::CardTypeRange;
    using Observable<ShufflingState>::subscribe;
    void notifyStateChange(const NotifyStateEvent&);
};

void SimpleCardManager::Impl::notifyStateChange(
    const NotifyStateEvent& event)
{
    notifyAll(event.state);
}

namespace {

using Impl = SimpleCardManager::Impl;
using CardTypeRange = Impl::CardTypeRange;

// This is event is defined here to be able to refer to CardTypeRange
class ShuffleEvent : public sc::event<ShuffleEvent> {
public:
    ShuffleEvent(CardTypeRange cards) :
        cards {cards}
    {
    }
    CardTypeRange cards;
};

class Idle : public sc::simple_state<Idle, Impl> {
public:
    using reactions = sc::transition<RequestShuffleEvent, ShuffleRequested>;
};

class Active : public sc::simple_state<Active, Impl, ShuffleRequested> {
public:
    using reactions = sc::in_state_reaction<
        NotifyStateEvent, Impl, &Impl::notifyStateChange>;

    void addShuffledCards(const ShuffleEvent&);
    const CardVector& getCards() const;
private:
    CardVector cards;
};

void Active::addShuffledCards(const ShuffleEvent& event)
{
    cards = CardVector(event.cards.begin(), event.cards.end());
}

const CardVector& Active::getCards() const
{
    return cards;
}

class ShuffleRequested : public sc::state<ShuffleRequested, Active> {
public:
    using reactions = sc::transition<
        ShuffleEvent, ShuffleCompleted, Active, &Active::addShuffledCards>;

    ShuffleRequested(my_context ctx);
};

ShuffleRequested::ShuffleRequested(my_context ctx) :
    my_base {ctx}
{
    post_event(NotifyStateEvent {CardManager::ShufflingState::REQUESTED});
}

class ShuffleCompleted : public sc::state<ShuffleCompleted, Active> {
public:
    using reactions =  sc::transition<RequestShuffleEvent, ShuffleRequested>;

    ShuffleCompleted(my_context ctx);
};

ShuffleCompleted::ShuffleCompleted(my_context ctx) :
    my_base {ctx}
{
    post_event(NotifyStateEvent {CardManager::ShufflingState::COMPLETED});
}

const CardVector& getCardsFrom(const Impl& impl)
{
    const auto& state = impl.state_cast<const Active&>();
    return state.getCards();
}

}

SimpleCardManager::SimpleCardManager() :
    impl {std::make_unique<Impl>()}
{
    impl->initiate();
}

SimpleCardManager::~SimpleCardManager() = default;

void SimpleCardManager::handleSubscribe(
    std::weak_ptr<Observer<ShufflingState>> observer)
{
    assert(impl);
    impl->subscribe(std::move(observer));
}

void SimpleCardManager::handleRequestShuffle()
{
    assert(impl);
    impl->process_event(RequestShuffleEvent {});
}

std::unique_ptr<Hand> SimpleCardManager::handleGetHand(const IndexRange ns)
{
    assert(impl);
    const auto& cards = getCardsFrom(*impl);
    return std::make_unique<BasicHand>(
        containerAccessIterator(ns.begin(), cards),
        containerAccessIterator(ns.end(), cards));
}

std::size_t SimpleCardManager::handleGetNumberOfCards() const
{
    assert(impl);
    return getCardsFrom(*impl).size();
}

bool SimpleCardManager::handleIsShuffleCompleted() const
{
    assert(impl);
    return impl->state_cast<const ShuffleCompleted*>();
}

void SimpleCardManager::internalShuffle(CardTypeRange cards)
{
    assert(impl);
    impl->process_event(ShuffleEvent {std::move(cards)});
}

}
}
