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

find_package(Boost REQUIRED)

include_directories(include ${Boost_INCLUDE_DIR})

add_subdirectory(src)
add_subdirectory(test/src)

add_executable(bridge Main.cc)

target_link_libraries(bridge ${BRIDGE_LIBRARIES})
