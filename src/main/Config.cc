#include "main/Config.hh"
#include "Logging.hh"
#include "Utility.hh"

#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <tuple>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace Bridge {
namespace Main {

namespace {

constexpr std::size_t READ_CHUNK_SIZE = 4096;
struct LuaStreamReaderArgs {
    LuaStreamReaderArgs(std::istream& in) : in {in}, buf {} {};
    std::istream& in;
    std::array<char, READ_CHUNK_SIZE> buf;
};

const char* luaStreamReader(lua_State*, void* data, std::size_t* size)
{
    auto& args = *reinterpret_cast<LuaStreamReaderArgs*>(data);
    if (args.in) {
        args.in.read(args.buf.data(), args.buf.size());
        *size = args.in.gcount();
        return args.buf.data();
    }
    *size = 0;
    return nullptr;
}

void loadAndExecuteFromStream(lua_State* lua, std::istream& in)
{
    std::istream::sentry s {in, true};
    if (s) {
        const auto reader_args = std::make_unique<LuaStreamReaderArgs>(in);
        const auto error =
            lua_load(
                lua, luaStreamReader, reader_args.get(), "config", nullptr) ||
            lua_pcall(lua, 0, 0, 0);
        if (error) {
            log(LogLevel::ERROR, "Error while running config script: %s",
                lua_tostring(lua, -1));
            throw std::runtime_error {"Could not process config"};
        }
    } else {
        throw std::runtime_error {"Could not read config, bad stream"};
    }
}

std::string getKeyStringOrEmpty(lua_State* lua, const char* key)
{
    auto ret = std::string {};
    lua_getglobal(lua, key);
    if (const auto* str = lua_tostring(lua, -1)) {
        ret = str;
        if (ret.size() != Bridge::Messaging::EXPECTED_CURVE_KEY_SIZE) {
            log(LogLevel::WARNING, "%s unexpected length: %d", key, ret.size());
        }
    }
    lua_pop(lua, 1);
    return ret;
}

}

Config::Config() = default;

Config::Config(std::istream& in)
{
    log(LogLevel::INFO, "Reading configs");

    const auto& closer = lua_close;
    const auto lua = std::unique_ptr<lua_State, decltype(closer)> {
        luaL_newstate(), closer};
    luaL_openlibs(lua.get());
    loadAndExecuteFromStream(lua.get(), in);

    const auto secret_key = getKeyStringOrEmpty(lua.get(), "curve_secret_key");
    const auto public_key = getKeyStringOrEmpty(lua.get(), "curve_public_key");
    if (!secret_key.empty() && !public_key.empty()) {
        curveConfig = Messaging::CurveKeys {
            public_key, secret_key, public_key };
    }

    log(LogLevel::INFO, "Reading configs completed");
}

const Messaging::CurveKeys* Config::getCurveConfig() const
{
    return getPtr(curveConfig);
}

Config configFromPath(const std::string_view path)
{
    if (path.empty()) {
        return {};
    } else if (path == "-") {
        return {std::cin};
    }
    std::ifstream in {path.data()};
    return {in};

}

}
}
