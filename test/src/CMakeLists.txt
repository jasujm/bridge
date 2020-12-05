include(FetchContent)
include(GoogleTest)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

# The following snippets for including googletest is adapted from Henry
# Schreiner et al.: Modern CMake (https://cliutils.gitlab.io/modern-cmake/)
# released under CC BY-NC-SA 4.0
# (https://creativecommons.org/licenses/by-nc-sa/4.0/)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.10.0
  INSTALL_DIR "${CMAKE_BINARY_DIR}"
  CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}")
FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
  FetchContent_Populate(googletest)
  add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()
mark_as_advanced(
  BUILD_GMOCK BUILD_GTEST BUILD_SHARED_LIBS INSTALL_GTEST
  gmock_build_tests gtest_build_samples gtest_build_tests
  gtest_disable_pthreads gtest_force_shared_crt gtest_hide_internal_symbols)

set(BRIDGE_TEST_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/test/include)

add_library(test-common STATIC TestMain.cc ${PROJECT_SOURCE_DIR}/src/Logging.cc
  ${PROJECT_SOURCE_DIR}/src/Thread.cc)
target_include_directories(test-common PUBLIC
  ${BRIDGE_INCLUDE_DIR} ${BRIDGE_TEST_INCLUDE_DIR})
target_link_libraries(test-common PUBLIC gtest gmock)
target_compile_features(test-common PUBLIC ${BRIDGE_COMPILE_FEATURES})
set_property(TARGET test-common PROPERTY CXX_STANDARD ${BRIDGE_CXX_STANDARD})

function(add_bridge_test)
  set(OneValueArgs TARGET NAME)
  set(MultiValueArgs DEPENDS)
  cmake_parse_arguments(BRIDGE_TEST
    "" "${OneValueArgs}" "${MultiValueArgs}" ${ARGN})
  file(GLOB src *.cc)
  add_executable(${BRIDGE_TEST_TARGET} ${src})
  target_link_libraries(${BRIDGE_TEST_TARGET} ${BRIDGE_TEST_DEPENDS}
    test-common Threads::Threads)
  set_property(TARGET ${BRIDGE_TEST_TARGET} PROPERTY CXX_STANDARD
    ${BRIDGE_CXX_STANDARD})
  add_test(${BRIDGE_TEST_NAME} ${BRIDGE_TEST_TARGET})
endfunction()

add_subdirectory(bridge)
add_subdirectory(messaging)
add_subdirectory(engine)
add_subdirectory(main)

if (BRIDGE_BUILD_CARD_SERVER AND LibTMCG_FOUND AND Boost_COROUTINE_FOUND)
  add_subdirectory(coroutines)
  add_subdirectory(csmain)
endif()
