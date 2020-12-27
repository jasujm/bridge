/** \file
 *
 * Definition of Bridge::Blob
 */

#ifndef BLOB_HH_
#define BLOB_HH_

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <span>
#include <vector>

namespace Bridge {

/** \brief Arbitrary binary data
 */
class Blob : public std::vector<std::byte> {
public:
    using std::vector<std::byte>::vector;
};

/** \brief Non‐owning view to a sequence of bytes
 *
 * This class extends the standard <tt>std::span<const std::byte></tt> by adding
 * comparison operators that compare the contained bytes.
 */
class ByteSpan : public std::span<const std::byte>
{
public:
    using std::span<const std::byte>::span;
};

/** \brief Three way comparison for byte spans
 */
constexpr auto operator<=>(const ByteSpan& lhs, const ByteSpan& rhs)
{
    return std::lexicographical_compare_three_way(
        lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

/** \brief Equality operator for byte spans
 */
constexpr bool operator==(const ByteSpan& lhs, const ByteSpan& rhs)
{
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

/** \brief Convert blob to string
 *
 * \param bytes Contiguous sequence of byte‐like objects
 *
 * \return String object containing the same bytes as \p bytes
 */
template<typename ByteRange>
std::string blobToString(const ByteRange& bytes)
{
    static_assert(
        sizeof(*std::data(bytes)) == 1,
        "Argument must contain one byte values");
    return std::string(
        reinterpret_cast<const char*>(std::data(bytes)), std::size(bytes));
}

/** \brief Convert string to blob
 *
 * \param string Contiguous sequence of char‐like objects
 *
 * \return Blob object containing the same bytes as \p string
 */
template<typename String>
Blob stringToBlob(const String& string)
{
    static_assert(
        sizeof(*std::data(string)) == 1,
        "Argument must contain one byte values");
    return Blob(
        reinterpret_cast<const std::byte*>(std::data(string)),
        reinterpret_cast<const std::byte*>(
            std::data(string)) + std::size(string));
}

/** \brief Get view to the bytes in object representation
 *
 * \param container A contiguous container
 *
 * \return ByteSpan object over the bytes in \p c
 */
template<typename Container>
ByteSpan asBytes(const Container& container)
{
    const auto* data = reinterpret_cast<const std::byte*>(std::data(container));
    const auto size =
        std::size(container) * sizeof(std::remove_pointer_t<decltype(data)>);
    return {data, static_cast<ByteSpan::size_type>(size)};
}

inline namespace BlobLiterals {

/** \brief Convert string literal into blob
 *
 * \param str the string literal
 * \param len the length of the string
 *
 * \return Blob with the characters of \p str interpreted as raw memory
 */
inline Blob operator"" _B(const char* str, std::size_t len)
{
    return Blob(
        reinterpret_cast<const std::byte*>(str),
        reinterpret_cast<const std::byte*>(str + len));
}

/** \brief Convert string literal into byte span
 *
 * \param str the string literal
 * \param len the length of the string
 *
 * \return Byte span with the characters of \p str interpreted as raw memory
 */
inline ByteSpan operator"" _BS(const char* str, std::size_t len)
{
    return ByteSpan(
        reinterpret_cast<const std::byte*>(str), len);
}

}

}

#endif // Bridge
