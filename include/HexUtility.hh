/** \file
 *
 * \brief Definition of utilities used to convert binary to/from hex
 */

#ifndef HEXUTILITY_HH_
#define HEXUTILITY_HH_

#include "Blob.hh"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <ostream>
#include <stdexcept>

namespace Bridge {

/// \cond DOXYGEN_IGNORE

namespace HexUtilityImpl {

inline constexpr std::array<char, 16> HEXS {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

inline constexpr std::array<int, 112> INVERT_HEXS {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

template<typename Integer>
bool isValidHexChar(Integer i)
{
    return i >= 0 && i < Integer {INVERT_HEXS.size()} && INVERT_HEXS[i] != -1;
}

template<typename Integer>
int nibble(Integer i)
{
    if (!isValidHexChar(i)) {
        throw std::runtime_error {
            "Invalid character in hex encoded string"};
    }
    return INVERT_HEXS[i];
}

template<typename OutputIterator>
struct OutputTypeExtractor {
    using type = typename std::iterator_traits<OutputIterator>::value_type;
};

template<typename Container>
struct OutputTypeExtractor<std::back_insert_iterator<Container>> {
    using type = typename Container::value_type;
};

}

/// \endcond

/** \brief Encode byte sequence as hex string
 *
 * This function reads raw byte input from range \p first to \p last, converts
 * it into hexadecimal string and writes the result to \p out.
 *
 * \tparam InputIterator Iterator to byte sequence. The value type of the
 * sequence must be one byte objects (char, std::byte etc.)
 * \tparam OutputIterator Iterator to character sequence the encoded value is
 * written to.
 *
 * \param first iterator to the first byte to encode
 * \param last iterator one past the last byte to encode
 * \param out the output iterator
 *
 * \return \p out
 */
template<typename InputIterator, typename OutputIterator>
OutputIterator encodeHex(InputIterator first, InputIterator last, OutputIterator out)
{
    using namespace HexUtilityImpl;
    using InputByte = typename std::iterator_traits<InputIterator>::value_type;
    static_assert(sizeof(InputByte) == sizeof(std::uint8_t), "Input must be byte sequence");
    while (first != last) {
        const auto c = static_cast<std::uint8_t>(*first++);
        *out++ = HEXS[(c & 0xf0) >> 4];
        *out++ = HEXS[c & 0x0f];
    }
    return out;
}

/** \brief Decode hex string into byte sequence
 *
 * This function reads hex encoded character sequence from range \p first to \p
 * last, converts it into raw byte sequence and writes the result to \p out. The
 * input sequence must have even number of characters 0–9 and a–f (case
 * insensitive) in ASCII compatible encoding.
 *
 * \note This function doesn’t provide strong exception guarantee because \p out
 * is not reset if invalid characters are encountered. isValidHex() can be
 * used to ensure beforehand that function invocation won’t throw.
 *
 * \tparam InputIterator iterator to the character sequence containing hex
 * encoded data
 * \tparam OutputIterator iterator to the sequence the raw bytes are written to
 *
 * \param first iterator to the first character to decode
 * \param last iterator one past the last character to decode
 * \param out the output iterator
 *
 * \return \p out
 *
 * \throw std::runtime_error if preconditions are violated (input sequence is
 * odd lenght or contains illegal characters).
 */
template<typename InputIterator, typename OutputIterator>
OutputIterator decodeHex(InputIterator first, InputIterator last, OutputIterator out)
{
    using namespace HexUtilityImpl;
    using OutputByte = typename OutputTypeExtractor<OutputIterator>::type;
    while (first != last) {
        const auto c1 = *first++;
        if (first == last) {
            throw std::runtime_error {
                "Hex encoded string must have even number of characters"};
        }
        const auto b1 = static_cast<OutputByte>(nibble(c1));
        const auto c2 = *first++;
        const auto b2 = static_cast<OutputByte>(nibble(c2));
        *out++ = (b1 << 4) | b2;
    }
    return out;
}

/** \brief Return sequence of bytes encoded as hex string
 *
 * This function is a wrapper over encodeHex() that accepts a range of bytes and
 * returns a pre‐allocated string containing the hex encoded bytes.
 *
 * \tparam Char the character type for the returned string
 *
 * \param bytes the bytes to encode
 *
 * \return A string containing \p bytes encoded as hexadecimal string
 */
template<typename Char = char>
auto toHex(const auto& bytes)
{
    using std::size;
    using std::begin;
    using std::end;
    auto ret = std::basic_string<Char> {};
    ret.reserve(2 * size(bytes));
    encodeHex(begin(bytes), end(bytes), std::back_inserter(ret));
    return ret;
}

/** \Brief Return a blob containing bytes from decoded hex string
 *
 * This function is a wrapper over decodeHex() that decodes a hex encoded string
 * and returns a pre‐allocated blob containing the bytes.
 *
 * \param string the string to decode
 *
 * \return A blob containing \p string decoded as hexadecimal
 *
 * \throw See decodeHex()
 */
auto fromHex(const auto& string)
{
    using std::size;
    using std::begin;
    using std::end;
    auto ret = Blob {};
    ret.reserve((size(string) + 1) / 2);
    decodeHex(begin(string), end(string), std::back_inserter(ret));
    return ret;
}

/** \brief Check if a range consists of valid hex string
 *
 * A string is considered a valid hex string if it has even number of characters
 * 0–9 and a–f (case insensitive) in ASCII compatible encoding.
 *
 * \param first iterator to the first character of the string
 * \param last iterator one past the last character of the string
 *
 * \return true if the string specified by \p first and \p last is valid hex
 * encoded string, false otherwise
 */
template<typename InputIterator>
bool isValidHex(InputIterator first, InputIterator last)
{
    return std::distance(first, last) % 2 == 0 &&
        std::all_of(
            first, last,
            [](const auto c) { return HexUtilityImpl::isValidHexChar(c); });
}

/// \cond DOXYGEN_IGNORE

namespace HexUtilityImpl {

template<typename Data>
struct HexFormatter {
    Data data;
};

template<typename Data>
std::ostream& operator<<(std::ostream& out, const HexFormatter<Data>& formatter)
{
    std::ostream::sentry s {out};
    if (s) {
        auto outiter = std::ostreambuf_iterator {out};
        encodeHex(
            std::begin(formatter.data), std::end(formatter.data), outiter);
    }
    return out;
}

}

/// \endcond

/** \brief Format binary data as hexadecimal
 *
 * This function returns an object that, when streamed to an output stream,
 * outputs hexadecimal representation of its underlying bytes. It is intended
 * to be used to print printable representation of binary data.
 *
 * \param data range (typically a blob) from which the data is read

 * \return hex formatter that can be streamed to std::ostream
 */
template<typename Data>
auto formatHex(Data&& data)
{
    return HexUtilityImpl::HexFormatter<Data> {std::forward<Data>(data)};
}


}

#endif // HEXUTILITY_HH_
