#include "bridge/CardTypeIterator.hh"

#include "bridge/BridgeConstants.hh"

#include <stdexcept>

namespace Bridge {

CardType enumerateCardType(const std::size_t n)
{
    static_assert(N_RANKS * N_SUITS == N_CARDS, "Unexpected number of cards");
    if (n >= N_CARDS) {
        throw std::domain_error("Invalid card number");
    }

    const auto n_suit = n / N_RANKS;
    const auto n_rank = n % N_RANKS;
    return {RANKS[n_rank], SUITS[n_suit]};
}

}