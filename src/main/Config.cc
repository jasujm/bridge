#include "main/Config.hh"

#include "bridge/Position.hh"
#include "messaging/EndpointIterator.hh"
#include "main/BridgeGameConfig.hh"
#include "IoUtility.hh"
#include "Logging.hh"
#include "Utility.hh"

#include <array>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <new>
#include <optional>
#include <stdexcept>

#include <boost/format.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace Bridge {
namespace Main {

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace {

struct GameConfigTag {};
auto GAME_CONFIG_TAG = GameConfigTag {};

constexpr auto CURVE_PUBLIC_KEY = "curve_public_key"sv;
constexpr auto CURVE_SECRET_KEY = "curve_secret_key"sv;

constexpr auto GAME_CONFIG_UUID = "uuid"sv;
constexpr auto GAME_CONFIG_POSITIONS_CONTROLLED = "positions_controlled"sv;
constexpr auto GAME_CONFIG_PEERS = "peers"sv;
constexpr auto GAME_CONFIG_ENDPOINT = "endpoint"sv;
constexpr auto GAME_CONFIG_SERVER_KEY = "server_key"sv;
constexpr auto GAME_CONFIG_CARD_SERVER = "card_server"sv;
constexpr auto GAME_CONFIG_CONTROL_ENDPOINT = "control_endpoint"sv;
constexpr auto GAME_CONFIG_PEER_ENDPOINT = "peer_endpoint"sv;
constexpr auto GAME_CONFIG_KNOWN_NODES = "known_nodes"sv;
constexpr auto GAME_CONFIG_PUBLIC_KEY = "public_key"sv;
constexpr auto GAME_CONFIG_USER_ID = "user_id"sv;

constexpr auto BIND_ADDRESS = "bind_address"sv;
constexpr auto BIND_BASE_PORT = "bind_base_port"sv;

const auto DEFAULT_BIND_ADDRESS = "*"s;
constexpr auto DEFAULT_BIND_BASE_ENDPOINT = 5555;

constexpr auto DATA_DIR = "data_dir"sv;

class LuaPopGuard {
public:
    LuaPopGuard(lua_State* lua);
    ~LuaPopGuard();
private:
    lua_State* lua;
};

LuaPopGuard::LuaPopGuard(lua_State* lua) :
    lua {lua}
{
}

LuaPopGuard::~LuaPopGuard()
{
    lua_pop(lua, 1);
}

constexpr auto READ_CHUNK_SIZE = 4096;
struct LuaStreamReaderArgs {
    LuaStreamReaderArgs(std::istream& in) : in {in}, buf {} {};
    std::istream& in;
    std::array<char, READ_CHUNK_SIZE> buf;
};

extern "C"
const char* config_lua_reader(
    lua_State*, void* data, std::size_t* size)
{
    auto& args = *static_cast<LuaStreamReaderArgs*>(data);
    if (args.in) {
        errno = 0;
        args.in.read(args.buf.data(), args.buf.size());
        if (args.in.bad()) {
            // Would throw exception here but this is extern "C"...
            log(LogLevel::WARNING, "Failed to read config: %s", strerror(errno));
        } else {
            *size = args.in.gcount();
            return args.buf.data();
        }
    }
    *size = 0;
    return nullptr;
}

void loadAndExecuteFromStream(lua_State* lua, std::istream& in)
{
    std::istream::sentry s {in, true};
    if (s) {
        const auto reader_args = std::make_unique<LuaStreamReaderArgs>(in);
        auto error = lua_load(
            lua, config_lua_reader, reader_args.get(), "config", nullptr);
        if (!error) {
            // Just for the duration of executing lots and lots of C
            // code... let's just die if out of memory instead of handling the
            // exception
            const auto out_of_memory_handler =
                std::set_new_handler(std::terminate);
            error = lua_pcall(lua, 0, 0, 0);
            std::set_new_handler(out_of_memory_handler);
        }
        if (error) {
            log(LogLevel::ERROR, "Error while running config script: %s",
                lua_tostring(lua, -1));
            throw std::runtime_error {"Could not process config"};
        }
    } else {
        log(LogLevel::ERROR, "Bad stream while reading config: %s", strerror(errno));
        throw std::runtime_error {"Failed to read config"};
    }
}

Blob getKeyOrEmpty(lua_State* lua, std::string_view key)
{
    lua_getglobal(lua, key.data());
    LuaPopGuard guard {lua};
    if (const auto* str = lua_tostring(lua, -1)) {
        auto ret = Messaging::decodeKey(str);
        if (ret.empty()) {
            log(LogLevel::WARNING, "Failed to decode %s", key);
        } else {
            return ret;
        }
    }
    return {};
}

std::optional<std::string> getString(lua_State* lua, std::string_view key)
{
    lua_getglobal(lua, key.data());
    LuaPopGuard guard {lua};
    if (const auto* str = lua_tostring(lua, -1)) {
        return str;
    } else if (!lua_isnoneornil(lua, -1)) {
        log(LogLevel::WARNING, "Expected string: %s", key);
    }
    return std::nullopt;
}

std::optional<int> getInt(lua_State* lua, std::string_view key)
{
    lua_getglobal(lua, key.data());
    LuaPopGuard guard {lua};
    auto success = 0;
    auto ret = 0;
    ret = lua_tointegerx(lua, -1, &success);
    if (success) {
        return ret;
    } else if (!lua_isnoneornil(lua, -1)) {
        log(LogLevel::WARNING, "Expected integer: %s", key);
    }
    return std::nullopt;
}

extern "C"
int config_lua_game(lua_State* lua) {
    // Lua uses longjmp to handle errors. Care must be taken that non-trivially
    // destructible objects are not in scope when calling Lua API functions that
    // can potentially cause errors.

    luaL_checktype(lua, 1, LUA_TTABLE);
    lua_pushlightuserdata(lua, &GAME_CONFIG_TAG);
    lua_rawget(lua, LUA_REGISTRYINDEX);
    auto& configVector = *static_cast<Config::GameConfigVector*>(
        lua_touserdata(lua, -1));
    auto& config = configVector.emplace_back();

    lua_pushstring(lua, GAME_CONFIG_UUID.data());
    lua_rawget(lua, 1);
    {
        const auto* uuid_string = lua_tostring(lua, -1);
        if (!uuid_string) {
            configVector.pop_back();
            luaL_error(lua, "expected argument to contain uuid");
        }
        try {
            auto uuid_generator = boost::uuids::string_generator {};
            config.uuid = uuid_generator(uuid_string);
        } catch (...) {
            configVector.pop_back();
            luaL_error(lua, "invalid uuid");
        }
    }
    lua_pop(lua, 1);

    lua_pushstring(lua, GAME_CONFIG_POSITIONS_CONTROLLED.data());
    if (lua_rawget(lua, 1) == LUA_TTABLE) {
        for (auto i = 1;; ++i) {
            lua_rawgeti(lua, -1, i);
            if (lua_isnil(lua, -1)) {
                lua_pop(lua, 1);
                break;
            }
            const auto* position_string = lua_tostring(lua, -1);
            if (!position_string) {
                configVector.pop_back();
                luaL_error(lua, "unexpected position value");
            }
            if (const auto position = Position::from(position_string)) {
                config.positionsControlled.emplace_back(*position);
                lua_pop(lua, 1);
            } else {
                configVector.pop_back();
                luaL_error(lua, "unexpected position string");
            }
        }
    }
    lua_pop(lua, 1);

    lua_pushstring(lua, GAME_CONFIG_PEERS.data());
    if (lua_rawget(lua, 1) == LUA_TTABLE) {
        for (auto i = 1;; ++i) {
            lua_rawgeti(lua, -1, i);
            if (lua_isnil(lua, -1)) {
                lua_pop(lua, 1);
                break;
            }
            if (!lua_istable(lua, -1)) {
                configVector.pop_back();
                luaL_error(lua, "expected peer descriptor to be table");
            }
            lua_pushstring(lua, GAME_CONFIG_ENDPOINT.data());
            lua_rawget(lua, -2);
            const auto* peer_endpoint = lua_tostring(lua, -1);
            if (!peer_endpoint) {
                configVector.pop_back();
                luaL_error(lua, "expected peer endpoint");
            }
            lua_pop(lua, 1);
            lua_pushstring(lua, GAME_CONFIG_SERVER_KEY.data());
            lua_rawget(lua, -2);
            auto peer_server_key = Blob {};
            if (const auto* encoded_key = lua_tostring(lua, -1)) {
                auto decoded_key = Messaging::decodeKey(encoded_key);
                if (decoded_key.empty()) {
                    configVector.pop_back();
                    luaL_error(lua, "failed to decode peer server key");
                }
                peer_server_key = std::move(decoded_key);
            }
            config.peers.emplace_back(
                BridgeGameConfig::PeerConfig {peer_endpoint, peer_server_key});
            lua_pop(lua, 2);
        }
    }
    lua_pop(lua, 1);

    lua_pushstring(lua, GAME_CONFIG_CARD_SERVER.data());
    if (lua_rawget(lua, 1) == LUA_TTABLE) {
        lua_pushstring(lua, GAME_CONFIG_CONTROL_ENDPOINT.data());
        lua_rawget(lua, -2);
        const auto* control_endpoint = lua_tostring(lua, -1);
        if (!control_endpoint) {
            configVector.pop_back();
            luaL_error(lua, "expected card server control endpoint");
        }
        lua_pop(lua, 1);
        lua_pushstring(lua, GAME_CONFIG_PEER_ENDPOINT.data());
        lua_rawget(lua, -2);
        const auto* peer_endpoint = lua_tostring(lua, -1);
        if (!peer_endpoint) {
            configVector.pop_back();
            luaL_error(lua, "expected card server peer endpoint");
        }
        config.cardServer = BridgeGameConfig::CardServerConfig {
            control_endpoint, peer_endpoint,
            getKeyOrEmpty(lua, CURVE_PUBLIC_KEY.data()) };
        lua_pop(lua, 2);
    }
    lua_pop(lua, 1);

    return 0;
}

}

class Config::Impl {
public:

    Impl();
    Impl(std::istream& in);

    Messaging::EndpointIterator getEndpointIterator() const;
    const Messaging::CurveKeys* getCurveConfig() const;
    std::optional<std::string_view> getDataDir() const;
    const GameConfigVector& getGameConfigs() const;
    const Messaging::Authenticator::NodeMap& getKnownNodes() const;

private:

    void createBaseEndpointConfig(lua_State* lua);
    void createCurveConfig(lua_State* lua);
    void createKnownNodesConfig(lua_State* lua);

    Messaging::EndpointIterator endpointIterator {
        DEFAULT_BIND_ADDRESS, DEFAULT_BIND_BASE_ENDPOINT};
    std::optional<Messaging::CurveKeys> curveConfig {};
    std::optional<std::string> dataDir {};
    GameConfigVector gameConfigs {};
    Messaging::Authenticator::NodeMap knownNodes {};
};

Config::Impl::Impl() = default;

Config::Impl::Impl(std::istream& in)
{
    log(LogLevel::INFO, "Reading configs");

    const auto& closer = lua_close;
    const auto lua = std::unique_ptr<lua_State, decltype(closer)> {
        luaL_newstate(), closer};
    luaL_openlibs(lua.get());

    lua_pushcfunction(lua.get(), config_lua_game);
    lua_setglobal(lua.get(), "game");

    lua_pushlightuserdata(lua.get(), &GAME_CONFIG_TAG);
    lua_pushlightuserdata(lua.get(), &gameConfigs);
    lua_settable(lua.get(), LUA_REGISTRYINDEX);

    loadAndExecuteFromStream(lua.get(), in);

    createBaseEndpointConfig(lua.get());
    createCurveConfig(lua.get());
    dataDir = getString(lua.get(), DATA_DIR);
    createKnownNodesConfig(lua.get());

    for (const auto& game_config : gameConfigs) {
        for (const auto& peer : game_config.peers) {
            if (!peer.serverKey.empty()) {
                const auto [peer_iter, created] =
                    knownNodes.try_emplace(peer.serverKey);
                if (created) {
                    const auto user_id =
                        boost::format("peer%1%") % knownNodes.size();
                    peer_iter->second = user_id.str();
                }
            }
        }
    }

    log(LogLevel::INFO, "Reading configs completed");
}

void Config::Impl::createBaseEndpointConfig(lua_State* lua)
{
    auto bind_address = getString(lua, BIND_ADDRESS);
    auto bind_base_port = getInt(lua, BIND_BASE_PORT);
    if (bind_address || bind_base_port) {
        endpointIterator = {
            bind_address.value_or(DEFAULT_BIND_ADDRESS),
            bind_base_port.value_or(DEFAULT_BIND_BASE_ENDPOINT)};
    }
}

void Config::Impl::createCurveConfig(lua_State* lua)
{
    const auto secret_key = getKeyOrEmpty(lua, CURVE_SECRET_KEY.data());
    const auto public_key = getKeyOrEmpty(lua, CURVE_PUBLIC_KEY.data());
    if (!secret_key.empty() && !public_key.empty()) {
        curveConfig = Messaging::CurveKeys { secret_key, public_key };
    }
}

void Config::Impl::createKnownNodesConfig(lua_State* lua)
{
    lua_getglobal(lua, GAME_CONFIG_KNOWN_NODES.data());
    LuaPopGuard guard {lua};
    if (lua_istable(lua, -1)) {
        for (auto i = 1;; ++i) {
            LuaPopGuard guard2 {lua};
            lua_rawgeti(lua, -1, i);
            if (lua_isnil(lua, -1)) {
                break;
            }
            if (!lua_istable(lua, -1)) {
                log(LogLevel::WARNING,
                    "known_nodes: expected node to be a table");
            }
            auto decoded_public_key = Blob {};
            {
                lua_pushstring(lua, GAME_CONFIG_PUBLIC_KEY.data());
                LuaPopGuard guard3 {lua};
                lua_rawget(lua, -2);
                const auto* const public_key = lua_tostring(lua, -1);
                if (!public_key) {
                    log(LogLevel::WARNING,
                        "known_nodes: expected node to have public_key");
                } else {
                    decoded_public_key = Messaging::decodeKey(public_key);
                    if (decoded_public_key.empty()) {
                        log(LogLevel::WARNING,
                            "known_nodes: expected public key, got %s",
                            public_key);
                    }
                }
            }
            const auto* user_id = static_cast<const char*>(nullptr);
            {
                lua_pushstring(lua, GAME_CONFIG_USER_ID.data());
                LuaPopGuard guard4 {lua};
                lua_rawget(lua, -2);
                user_id = lua_tostring(lua, -1);
                if (!user_id) {
                    log(LogLevel::WARNING,
                        "known_nodes: expected node to have user_id");
                }
            }
            if (user_id && !decoded_public_key.empty()) {
                knownNodes.try_emplace(std::move(decoded_public_key), user_id);
            }
        }
    }
}

Messaging::EndpointIterator Config::Impl::getEndpointIterator() const
{
    return endpointIterator;
}

const Messaging::CurveKeys* Config::Impl::getCurveConfig() const
{
    return getPtr(curveConfig);
}

std::optional<std::string_view> Config::Impl::getDataDir() const
{
    return dataDir;
}

const Config::GameConfigVector& Config::Impl::getGameConfigs() const
{
    return gameConfigs;
}

const Messaging::Authenticator::NodeMap& Config::Impl::getKnownNodes() const
{
    return knownNodes;
}

Config::Config() :
    impl {std::make_unique<Impl>()}
{
}

Config::Config(std::istream& in) :
    impl {std::make_unique<Impl>(in)}
{
}

Config::Config(Config&&) = default;

Config::~Config() = default;

Config& Config::operator=(Config&&) = default;

Messaging::EndpointIterator Config::getEndpointIterator() const
{
    assert(impl);
    return impl->getEndpointIterator();
}

const Messaging::CurveKeys* Config::getCurveConfig() const
{
    assert(impl);
    return impl->getCurveConfig();
}

std::optional<std::string_view> Config::getDataDir() const
{
    assert(impl);
    return impl->getDataDir();
}

const Config::GameConfigVector& Config::getGameConfigs() const
{
    assert(impl);
    return impl->getGameConfigs();
}

const Messaging::Authenticator::NodeMap& Config::getKnownNodes() const
{
    assert(impl);
    return impl->getKnownNodes();
}

Config configFromPath(const std::string_view path)
{
    if (path.empty()) {
        return {};
    } else {
        errno = 0;
        return processStreamFromPath(
            path, [](auto& in) { return Config {in}; });
    }
}

}
}
