/** \file
 *
 * \brief Definition of Bridge::Main::Config class
 */

#include "messaging/Security.hh"

#include <iosfwd>
#include <optional>
#include <string>

namespace Bridge {
namespace Main {

/** \brief Configuration file processing utility
 *
 * \todo documentation
 */
class Config {
public:

    /** \brief Create empty configs
     */
    Config();

    /** \brief Create configuration from stream
     *
     * The constructor reads configuration script from stream \p in and
     * processes it. The processing involves reading the stream until EOF,
     * parsing the contents as Lua script and running the script.
     *
     * \throw std::runtime_error if reading the stream or processing the script
     * fails
     */
    Config(std::istream& in);

    /** \brief Get CurveZMQ keys
     *
     * \return pointer to the curve keys in the configs, or nullptr if no keys
     * are available
     */
    const Messaging::CurveKeys* getCurveConfig() const;

private:

    std::optional<Messaging::CurveKeys> curveConfig;
};

/** \brief Create configuration from file
 *
 * Depending on the value the \p path, the function generates the config object
 * in different ways:
 * - If \p path is empty, empty configuration is returned
 * - If \p path is dash (“-”), configuration is read from stdin
 * - Otherwise \p path is interpreted as path to the configuration file
 *
 * \param path the path of the configuration file
 *
 * \return config object based on the file
 */
Config configFromPath(std::string_view path);

}
}
