/** \file
 *
 * \brief Definition of Bridge::CardType struct and related concepts
 */

#ifndef CARDTYPE_HH_
#define CARDTYPE_HH_

#include <boost/bimap/bimap.hpp>
#include <boost/operators.hpp>

#include <array>
#include <iosfwd>
#include <string>

namespace Bridge {

/** \brief Playing card rank
 */
enum class Rank {
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
    ACE
};

/** \brief Playing card suit
 */
enum class Suit {
    CLUBS,
    DIAMONDS,
    HEARTS,
    SPADES
};

/** \brief Number of playing card ranks
 *
 * \sa Rank
 */
constexpr auto N_RANKS = 13;

/** \brief Number of playing card suits
 *
 * \sa Suit
 */
constexpr auto N_SUITS = 4;

/** \brief Array containing all playing card ranks
 *
 * \sa Rank
 */
constexpr std::array<Rank, N_RANKS> RANKS {
    Rank::TWO,
    Rank::THREE,
    Rank::FOUR,
    Rank::FIVE,
    Rank::SIX,
    Rank::SEVEN,
    Rank::EIGHT,
    Rank::NINE,
    Rank::TEN,
    Rank::JACK,
    Rank::QUEEN,
    Rank::KING,
    Rank::ACE,
};

/** \brief Array containing all playing card suits
 *
 * \sa Suit
 */
constexpr std::array<Suit, N_SUITS> SUITS {
    Suit::CLUBS,
    Suit::DIAMONDS,
    Suit::HEARTS,
    Suit::SPADES,
};

/** \brief Type of \ref RANK_TO_STRING_MAP
 */
using RankToStringMap = boost::bimaps::bimap<Rank, std::string>;

/** \brief Two-way map between Rank enumerations and their string
 * representation
 */
extern const RankToStringMap RANK_TO_STRING_MAP;

/** \brief Type of \ref SUIT_TO_STRING_MAP
 */
using SuitToStringMap = boost::bimaps::bimap<Suit, std::string>;

/** \brief Two-way map between Suit enumerations and their string
 * representation
 */
extern const SuitToStringMap SUIT_TO_STRING_MAP;

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

/** \brief Input a Rank from stream
 *
 * \param is the input stream
 * \param rank the rank to input
 *
 * \return parameter \p is
 */
std::istream& operator>>(std::istream& is, Rank& rank);

/** \brief Input a Suit from stream
 *
 * \param is the input stream
 * \param suit the suit to input
 *
 * \return parameter \p is
 */
std::istream& operator>>(std::istream& is, Suit& suit);

}

#endif // CARDTYPE_HH_
