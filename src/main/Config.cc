#include "main/Config.hh"

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

namespace {

struct GameConfigTag {};
GameConfigTag gameConfigTag;

const auto GAME_CONFIG_UUID = "uuid"s;

const auto DEFAULT_BIND_ADDRESS = "*"s;
const auto DEFAULT_BIND_BASE_ENDPOINT = 5555;

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

constexpr std::size_t READ_CHUNK_SIZE = 4096;
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

Blob getKeyOrEmpty(lua_State* lua, const char* key)
{
    lua_getglobal(lua, key);
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

std::optional<std::string> getString(lua_State* lua, const char* key)
{
    lua_getglobal(lua, key);
    LuaPopGuard guard {lua};
    if (const auto* str = lua_tostring(lua, -1)) {
        return str;
    } else {
        log(LogLevel::WARNING, "Expected string: %s", key);
    }
    return std::nullopt;
}

std::optional<int> getInt(lua_State* lua, const char* key)
{
    lua_getglobal(lua, key);
    LuaPopGuard guard {lua};
    auto success = 0;
    auto ret = 0;
    ret = lua_tointegerx(lua, -1, &success);
    if (success) {
        return ret;
    } else {
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
    lua_pushlightuserdata(lua, &gameConfigTag);
    lua_rawget(lua, LUA_REGISTRYINDEX);
    auto& configVector = *static_cast<Config::GameConfigVector*>(
        lua_touserdata(lua, -1));
    auto& config = configVector.emplace_back();

    lua_pushstring(lua, GAME_CONFIG_UUID.c_str());
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

    return 0;
}

}

class Config::Impl {
public:

    Impl();
    Impl(std::istream& in);

    Messaging::EndpointIterator getEndpointIterator() const;
    const Messaging::CurveKeys* getCurveConfig() const;
    const GameConfigVector& getGameConfigs() const;

private:

    void createBaseEndpointConfig(lua_State* lua);
    void createCurveConfig(lua_State* lua);

    Messaging::EndpointIterator endpointIterator {
        DEFAULT_BIND_ADDRESS, DEFAULT_BIND_BASE_ENDPOINT};
    std::optional<Messaging::CurveKeys> curveConfig {};
    GameConfigVector gameConfigs {};
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

    lua_pushlightuserdata(lua.get(), &gameConfigTag);
    lua_pushlightuserdata(lua.get(), &gameConfigs);
    lua_settable(lua.get(), LUA_REGISTRYINDEX);

    loadAndExecuteFromStream(lua.get(), in);

    createBaseEndpointConfig(lua.get());
    createCurveConfig(lua.get());

    log(LogLevel::INFO, "Reading configs completed");
}

void Config::Impl::createBaseEndpointConfig(lua_State* lua)
{
    auto bind_address = getString(lua, "bind_address");
    auto bind_base_port = getInt(lua, "bind_base_port");
    if (bind_address || bind_base_port) {
        endpointIterator = {
            bind_address.value_or(DEFAULT_BIND_ADDRESS),
            bind_base_port.value_or(DEFAULT_BIND_BASE_ENDPOINT)};
    }
}

void Config::Impl::createCurveConfig(lua_State* lua)
{
    const auto secret_key = getKeyOrEmpty(lua, "curve_secret_key");
    const auto public_key = getKeyOrEmpty(lua, "curve_public_key");
    if (!secret_key.empty() && !public_key.empty()) {
        curveConfig = Messaging::CurveKeys {
            public_key, secret_key, public_key };
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

const Config::GameConfigVector& Config::Impl::getGameConfigs() const
{
    return gameConfigs;
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

const Config::GameConfigVector& Config::getGameConfigs() const
{
    assert(impl);
    return impl->getGameConfigs();
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
