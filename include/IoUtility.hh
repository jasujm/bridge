/** \file
 *
 * \brief Definition of input and output stream related utilities
 *
 * Although the utilities in this do not depend on any other classes or
 * functions inside the Bridge namespace, the functions are still inside the
 * namespace to avoid name conflicts.
 */

#ifndef IOUTILITY_HH_
#define IOUTILITY_HH_

#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <string_view>
#include <variant>

namespace Bridge {

/** \brief Output optional value
 *
 * If \p t is not empty, outputs the wrapped value using \c operator<< for \c
 * T. Otherwise outputs the placeholder value “(none)”.
 *
 * \param os the output stream
 * \param t the value to be written to \p os
 */
template<typename T>
std::ostream& operator<<(std::ostream& os, const std::optional<T>& t)
{
    if (t) {
        return os << *t;
    }
    return os << "(none)";
}

/** \brief Output variant
 *
 * This function simply applies visitor which uses \c operator<< for the
 * underlying type.
 *
 * \param os the output stream
 * \param t the value to be written to \p os
 */
template<typename T, typename... Ts>
std::ostream& operator<<(std::ostream& os, const std::variant<T, Ts...>& t)
{
    std::visit([&os](const auto& v) { os << v; }, t);
    return os;
}

/** \brief Process file stream or stdin based on \p path
 *
 * This function first examines \p path. If \p path is a hyphen (“-”), calls \p
 * callback with \c std::cin as argument. Otherwise interprets \p path as a
 * filesystem path, opens the corresponding file and passes reference to the
 * corresponding \c std::ifstream to the callback. If this function opens a
 * file, it is closed after the callback returns.
 *
 * \param path a filesystem path or hyphen
 * \param callback a callable that accepts reference to \c std::istream as
 * argument
 *
 * \return the result of invoking \p callback with the reference to the stream
 */
template<typename Callable>
decltype(auto) processStreamFromPath(std::string_view path, Callable&& callback)
{
    auto helper = [&callback](auto& in) -> decltype(auto) {
        return std::invoke(std::forward<Callable>(callback), in);
    };
    if (path == "-") {
        return helper(std::cin);
    } else {
        auto in = std::ifstream {path.data()};
        return helper(in);
    }
}

}

#endif // IOUTILITY_HH_
