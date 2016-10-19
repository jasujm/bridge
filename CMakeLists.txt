cmake_minimum_required(VERSION 3.1)
project(Bridge)

enable_testing()

include(CMakeParseArguments)
include(FindPythonInterp)

set(BRIDGE_COMPILE_FEATURES cxx_aggregate_default_initializers cxx_auto_type
  cxx_constexpr cxx_contextual_conversions cxx_decltype_auto
  cxx_default_function_template_args cxx_defaulted_functions
  cxx_defaulted_move_initializers cxx_generalized_initializers
  cxx_generic_lambdas cxx_lambdas cxx_lambda_init_captures cxx_noexcept
  cxx_nonstatic_member_init cxx_nullptr cxx_override cxx_range_for
  cxx_return_type_deduction cxx_right_angle_brackets cxx_rvalue_references
  cxx_static_assert cxx_strong_enums cxx_trailing_return_types
  cxx_uniform_initialization cxx_variadic_templates)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED)
find_package(ZeroMQ REQUIRED)
find_package(Json REQUIRED)
find_package(LibTMCG)
find_package(PythonInterp)

include_directories(include ${Boost_INCLUDE_DIR} ${ZEROMQ_INCLUDE_DIRS}
  ${JSON_INCLUDE_DIRS})

if (${PYTHONINTERP_FOUND} AND NOT ${PYTHON_VERSION_MAJOR} LESS 3)
  set(BRIDGE_PYTHON_AVAILABLE ON)
endif()

add_subdirectory(src)
add_subdirectory(test)
if (${BRIDGE_PYTHON_AVAILABLE})
  add_subdirectory(python)
endif()

find_package(Doxygen)
if(DOXYGEN_FOUND)
  set(DOXYGEN_FILE ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
  configure_file(Doxyfile.in ${DOXYGEN_FILE} @ONLY)
  add_custom_target(doc COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_FILE})
endif()
