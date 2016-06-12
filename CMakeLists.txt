cmake_minimum_required(VERSION 3.1)
project(Bridge)

enable_testing()

include(CMakeParseArguments)

set(BRIDGE_COMPILE_FEATURES cxx_auto_type cxx_constexpr cxx_decltype_auto
  cxx_defaulted_functions cxx_generalized_initializers cxx_generic_lambdas
  cxx_lambdas cxx_noexcept cxx_nonstatic_member_init cxx_override
  cxx_range_for cxx_return_type_deduction cxx_right_angle_brackets
  cxx_rvalue_references cxx_static_assert cxx_strong_enums
  cxx_trailing_return_types cxx_variadic_templates)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED)
find_package(ZeroMQ REQUIRED)
find_package(Json REQUIRED)
find_package(LibTMCG)

include_directories(include ${Boost_INCLUDE_DIR} ${ZEROMQ_INCLUDE_DIRS}
  ${JSON_INCLUDE_DIRS})

add_subdirectory(src)
add_subdirectory(test/src)

add_executable(bridge Bridge.cc)
target_link_libraries(bridge bridge-main bridge-messaging bridge-engine
  bridge-scoring bridge-bridge ${ZEROMQ_LIBRARIES} Threads::Threads)

if(LibTMCG_FOUND)
  add_executable(cardserver CardServer.cc)
  target_link_libraries(cardserver bridge-cardserver bridge-messaging
    ${ZEROMQ_LIBRARIES} ${TMCG_LIBRARIES} Threads::Threads)
endif()
