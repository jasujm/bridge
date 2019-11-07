/** \file
 *
 * Definition of Bridge::Blob
 */

#ifndef BLOB_HH_
#define BLOB_HH_

#include <boost/operators.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <vector>

namespace Bridge {

/** \brief The preferred data type for holding arbitrary length binary data
 */
using Blob = std::vector<std::byte>;

/** \brief Non‐owning view to a sequence of bytes
 *
 * This class implements a restricted subset of functionality of the span class
 * proposed for the future C++ standard
 * (http://open-std.org/JTC1/SC22/WG21/docs/papers/2016/p0122r1.pdf). In
 * particular it’s implemented only for const std::byte and supports only
 * dynamic extent.
 */
class ByteSpan : private boost::totally_ordered<ByteSpan>
{
public:

    using element_type = const std::byte;    ///< Element type
    using value_type = std::byte;            ///< Value type
    using index_type = std::ptrdiff_t;       ///< Index type
    using difference_type = std::ptrdiff_t;  ///< Difference type
    using pointer = const std::byte*;        ///< Pointer type
    using reference = const std::byte&;      ///< Reference type
    using iterator = const std::byte*;       ///< Iterator type
    using const_iterator = const std::byte*; ///< Const iterator type

    /** \brief Default constructor
     *
     * Create byte span containing no bytes
     */
    constexpr ByteSpan() noexcept = default;

    /** \brief Create byte span in given range
     *
     * Create byte span in range <tt>[ptr…ptr+count]</tt> (exclusive).
     *
     * \param ptr pointer to the beginning of the span
     * \param count number of bytes spanned
     */
    constexpr ByteSpan(pointer ptr, index_type count) noexcept :
        ptr {ptr},
        count {count}
    {
    }

    /** \brief Copy constructor
     */
    constexpr ByteSpan(const ByteSpan&) noexcept = default;

    /** \brief Create byte span over container
     *
     * \param c A container of objects convertible to std::byte
     */
    template<
        typename Container,
        typename = std::enable_if_t<
            std::is_convertible_v<decltype(std::data(std::declval<const Container>())), pointer> &&
            std::is_convertible_v<decltype(std::size(std::declval<const Container>())), index_type>>>
    constexpr ByteSpan(const Container& c) :
        ptr {static_cast<pointer>(std::data(c))},
        count {static_cast<index_type>(std::size(c))}
    {
    }

    /** \brief Copy assignment
     */
    constexpr ByteSpan& operator=(const ByteSpan&) noexcept = default;

    /** \brief Get pointer to the beginning of the span
     *
     * \return pointer to the beginning of the span
     */
    constexpr pointer data() const noexcept
    {
        return ptr;
    }

    /** \brief Get size of the span
     *
     * \return Size of the span
     */
    constexpr index_type size() const noexcept
        {
            return count;
        }

    /** \brief Get iterator to the beginning of the span
     *
     * \return iterator to the beginning of the span
     */
    constexpr iterator begin() const noexcept
    {
        return data();
    }

    /** \brief Get iterator to one past the end of the span
     *
     * \return iterator to one past the end of the span
     */
    constexpr iterator end() const noexcept
    {
        return data() + size();
    }

private:
    pointer ptr {};
    index_type count {};
};

/** \brief Equality operator for byte spans
 */
inline bool operator==(const ByteSpan& lhs, const ByteSpan& rhs)
{
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

/** \brief Less than operator for byte spans
 */
inline bool operator<(const ByteSpan& lhs, const ByteSpan& rhs)
{
    return std::lexicographical_compare(
        lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
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
    return {data, static_cast<ByteSpan::index_type>(size)};
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
