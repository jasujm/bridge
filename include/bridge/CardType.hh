/** \file
 *
 * \brief Definition of Bridge::CardType struct and related concepts
 */

#ifndef CARDTYPE_HH_
#define CARDTYPE_HH_

#include "enhanced_enum/enhanced_enum.hh"

#include <boost/operators.hpp>

#include <iosfwd>
#include <string_view>

namespace Bridge {

/// \cond internal

/*[[[cog
import cog
import enumecg
import enum
class Rank(enum.Enum):
  TWO = "2"
  THREE = "3"
  FOUR = "4"
  FIVE = "5"
  SIX = "6"
  SEVEN = "7"
  EIGHT = "8"
  NINE = "9"
  TEN = "10"
  JACK = "jack"
  QUEEN = "queen"
  KING = "king"
  ACE = "ace"
cog.out(enumecg.generate(Rank, primary_type="enhanced"))
cog.out("\n\n")
class Suit(enum.Enum):
  CLUBS = "clubs"
  DIAMONDS = "diamonds"
  HEARTS = "hearts"
  SPADES = "spades"
cog.out(enumecg.generate(Suit, primary_type="enhanced"))
]]]*/
enum class RankLabel {
    TWO,
    THREE,
    FOUR,
    FIVE,
    SIX,
    SEVEN,
    EIGHT,
    NINE,
    TEN,
    JACK,
    QUEEN,
    KING,
    ACE,
};

struct Rank : ::enhanced_enum::enum_base<Rank, RankLabel, std::string_view> {
    using ::enhanced_enum::enum_base<Rank, RankLabel, std::string_view>::enum_base;
    static constexpr std::array values {
        value_type { "2" },
        value_type { "3" },
        value_type { "4" },
        value_type { "5" },
        value_type { "6" },
        value_type { "7" },
        value_type { "8" },
        value_type { "9" },
        value_type { "10" },
        value_type { "jack" },
        value_type { "queen" },
        value_type { "king" },
        value_type { "ace" },
    };
};

constexpr Rank enhance(RankLabel e) noexcept
{
    return e;
}

namespace Ranks {
inline constexpr const Rank::value_type& TWO_VALUE { std::get<0>(Rank::values) };
inline constexpr const Rank::value_type& THREE_VALUE { std::get<1>(Rank::values) };
inline constexpr const Rank::value_type& FOUR_VALUE { std::get<2>(Rank::values) };
inline constexpr const Rank::value_type& FIVE_VALUE { std::get<3>(Rank::values) };
inline constexpr const Rank::value_type& SIX_VALUE { std::get<4>(Rank::values) };
inline constexpr const Rank::value_type& SEVEN_VALUE { std::get<5>(Rank::values) };
inline constexpr const Rank::value_type& EIGHT_VALUE { std::get<6>(Rank::values) };
inline constexpr const Rank::value_type& NINE_VALUE { std::get<7>(Rank::values) };
inline constexpr const Rank::value_type& TEN_VALUE { std::get<8>(Rank::values) };
inline constexpr const Rank::value_type& JACK_VALUE { std::get<9>(Rank::values) };
inline constexpr const Rank::value_type& QUEEN_VALUE { std::get<10>(Rank::values) };
inline constexpr const Rank::value_type& KING_VALUE { std::get<11>(Rank::values) };
inline constexpr const Rank::value_type& ACE_VALUE { std::get<12>(Rank::values) };
inline constexpr Rank TWO { RankLabel::TWO };
inline constexpr Rank THREE { RankLabel::THREE };
inline constexpr Rank FOUR { RankLabel::FOUR };
inline constexpr Rank FIVE { RankLabel::FIVE };
inline constexpr Rank SIX { RankLabel::SIX };
inline constexpr Rank SEVEN { RankLabel::SEVEN };
inline constexpr Rank EIGHT { RankLabel::EIGHT };
inline constexpr Rank NINE { RankLabel::NINE };
inline constexpr Rank TEN { RankLabel::TEN };
inline constexpr Rank JACK { RankLabel::JACK };
inline constexpr Rank QUEEN { RankLabel::QUEEN };
inline constexpr Rank KING { RankLabel::KING };
inline constexpr Rank ACE { RankLabel::ACE };
}

enum class SuitLabel {
    CLUBS,
    DIAMONDS,
    HEARTS,
    SPADES,
};

struct Suit : ::enhanced_enum::enum_base<Suit, SuitLabel, std::string_view> {
    using ::enhanced_enum::enum_base<Suit, SuitLabel, std::string_view>::enum_base;
    static constexpr std::array values {
        value_type { "clubs" },
        value_type { "diamonds" },
        value_type { "hearts" },
        value_type { "spades" },
    };
};

constexpr Suit enhance(SuitLabel e) noexcept
{
    return e;
}

namespace Suits {
inline constexpr const Suit::value_type& CLUBS_VALUE { std::get<0>(Suit::values) };
inline constexpr const Suit::value_type& DIAMONDS_VALUE { std::get<1>(Suit::values) };
inline constexpr const Suit::value_type& HEARTS_VALUE { std::get<2>(Suit::values) };
inline constexpr const Suit::value_type& SPADES_VALUE { std::get<3>(Suit::values) };
inline constexpr Suit CLUBS { SuitLabel::CLUBS };
inline constexpr Suit DIAMONDS { SuitLabel::DIAMONDS };
inline constexpr Suit HEARTS { SuitLabel::HEARTS };
inline constexpr Suit SPADES { SuitLabel::SPADES };
}
//[[[end]]]

/// \endcond

/** \brief Playing card type
 *
 * CardType objects are equality comparable. They compare equal when both rank
 * and suit are equal.
 *
 * \note Boost operators library is used to ensure that operator!= is
 * generated with usual semantics when operator== is supplied.
 */
struct CardType : private boost::equality_comparable<CardType> {
    Rank rank;  ///< \brief Rank of the card
    Suit suit;  ///< \brief Suit of the card

    CardType() = default;

    /** \brief Create new card type
     *
     * \param rank the rank of the card
     * \param suit the suit of the card
     */
    constexpr CardType(Rank rank, Suit suit) :
        rank {rank},
        suit {suit}
    {
    }
};

/** \brief Equality operator for card types
 *
 * \sa CardType
 */
bool operator==(const CardType&, const CardType&);

/** \brief Output a Rank to stream
 *
 * \param os the output stream
 * \param rank the rank to output
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, Rank rank);

/** \brief Output a Suit to stream
 *
 * \param os the output stream
 * \param suit the suit to output
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, Suit suit);

/** \brief Output a CardType to stream
 *
 * \param os the output stream
 * \param cardType the card type to output
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, CardType cardType);

}

#endif // CARDTYPE_HH_
