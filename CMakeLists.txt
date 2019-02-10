cmake_minimum_required(VERSION 3.11)
project(Bridge)

enable_testing()

include(CMakeParseArguments)
include(FindPythonInterp)

set(BRIDGE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS coroutine)
find_package(ZeroMQ REQUIRED)
find_package(Json REQUIRED)
find_package(Lua 5.3 REQUIRED)
find_package(LibTMCG)
find_package(PythonInterp)

include_directories(include ${Boost_INCLUDE_DIR})

if (${PYTHONINTERP_FOUND} AND NOT ${PYTHON_VERSION_MAJOR} LESS 3)
  set(BRIDGE_PYTHON_AVAILABLE ON)
endif()

add_subdirectory(src)
add_subdirectory(test)

find_package(Doxygen)
if(DOXYGEN_FOUND)
  set(DOXYGEN_FILE ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
  configure_file(Doxyfile.in ${DOXYGEN_FILE} @ONLY)
  add_custom_target(doc COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_FILE})
endif()
