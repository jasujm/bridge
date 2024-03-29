cmake_minimum_required(VERSION 3.11)
project(Bridge)

enable_testing()

include(CMakeParseArguments)

set(BRIDGE_CXX_STANDARD 20)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(Boost 1.74 REQUIRED OPTIONAL_COMPONENTS coroutine)
find_package(cppzmq 4.8 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Lua 5.4 REQUIRED)
find_package(RocksDB)
find_package(LibTMCG)

add_subdirectory(external/enhanced-enum)

set(BRIDGE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_subdirectory(src)

option(BRIDGE_BUILD_TESTS "Build tests" ON)
if (BRIDGE_BUILD_TESTS)
  add_subdirectory(test)
endif()

option(BRIDGE_BUILD_DOCS "Build documentation" ON)
if (BRIDGE_BUILD_DOCS)
  find_package(Doxygen)
  if(Doxygen_FOUND)
    set(DOXYGEN_FILE ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    configure_file(Doxyfile.in ${DOXYGEN_FILE} @ONLY)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/img DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/html)
    add_custom_target(docs COMMAND Doxygen::doxygen ${DOXYGEN_FILE})
  endif()
endif()
