/** \file
 *
 * \brief Definition of Bridge::Main::Config class
 */

#include "messaging/Authenticator.hh"
#include "messaging/Security.hh"

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace Bridge {

namespace Messaging {

class EndpointIterator;
struct CurveKeys;

}

namespace Main {

class BridgeGameConfig;

/** \brief Configuration file processing utility
 *
 * \todo Documentation of the available options is missing
 */
class Config {
public:

    /** \brief Vector of bridge game configurations
     */
    using GameConfigVector = std::vector<BridgeGameConfig>;

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

    /** \brief Move constructor
     */
    Config(Config&&);

    ~Config();

    /** \brief Move assignment
     */
    Config& operator=(Config&&);

    /** \brief Get endpoint iterator for control and event endpoints
     *
     * \return Endpoint iterator returning control and event endpoints as
     * successive elements
     *
     * \sa %Bridge protocol: \ref bridgeprotocolpeer
     */
    Messaging::EndpointIterator getEndpointIterator() const;

    /** \brief Get CurveZMQ keys
     *
     * \return pointer to the curve keys in the configs, or nullptr if no keys
     * are available
     */
    const Messaging::CurveKeys* getCurveConfig() const;

    /** \brief Get data directory
     *
     * \return Path to the data directory, or nullopt if there is no data
     * directory
     */
    std::optional<std::string_view> getDataDir() const;

    /** \brief Get bridge game configurations
     *
     * \return reference to bridge game configs
     */
    const GameConfigVector& getGameConfigs() const;

    /** \brief Get known peers
     *
     * \return mapping from known peer public keys to their user IDs
     */
    const Messaging::Authenticator::NodeMap& getKnownPeers() const;

private:

    struct Impl;
    std::unique_ptr<const Impl> impl;
};

/** \brief Create configuration from file
 *
 * Depending on the value the \p path, the function generates the config object
 * in different ways:
 * - If \p path is empty, empty configuration is returned
 * - If \p path is hyphen (“-”), configuration is read from stdin
 * - Otherwise \p path is interpreted as path to the configuration file
 *
 * \param path the path of the configuration file
 *
 * \return config object based on the file
 */
Config configFromPath(std::string_view path);

}
}
