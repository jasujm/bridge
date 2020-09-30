#include "bridge/CardTypeIterator.hh"

#include "bridge/BridgeConstants.hh"

#include <stdexcept>

namespace Bridge {

int cardTypeIndex(const CardType& card)
{
    const auto n_suit = static_cast<int>(card.suit.get());
    const auto n_rank = static_cast<int>(card.rank.get());
    return n_suit * Rank::size() + n_rank;
}

CardType enumerateCardType(const int n)
{
    static_assert(Rank::size() * Suit::size() == N_CARDS);
    if (n < 0 || n >= N_CARDS) {
        throw std::invalid_argument {"Invalid card number"};
    }

    constexpr auto N_RANKS = Rank::size();
    const auto n_suit = n / N_RANKS;
    const auto n_rank = n % N_RANKS;
    return {static_cast<RankLabel>(n_rank), static_cast<SuitLabel>(n_suit)};
}

}
