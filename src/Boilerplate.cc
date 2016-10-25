#include "Logging.hh"

#include <boost/core/demangle.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>

int bridge_main(int argc, char* argv[]);

int main(int argc, char* argv[])
{
    try {
        return bridge_main(argc, argv);
    } catch (std::exception& e) {
        log(
            Bridge::LogLevel::FATAL,
            "%s terminated with exception of type %s: %s",
            argv[0], boost::core::demangle(typeid(e).name()), e.what());
        return EXIT_FAILURE;
    }
}
