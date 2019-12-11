#include "bridge/CardTypeIterator.hh"

#include "bridge/BridgeConstants.hh"

#include <stdexcept>

namespace Bridge {

CardType enumerateCardType(const int n)
{
    static_assert(Rank::size() * Suit::size() == N_CARDS);
    if (n >= N_CARDS) {
        throw std::invalid_argument {"Invalid card number"};
    }

    constexpr auto N_RANKS = Rank::size();
    const auto n_suit = n / N_RANKS;
    const auto n_rank = n % N_RANKS;
    return {static_cast<RankLabel>(n_rank), static_cast<SuitLabel>(n_suit)};
}

}
