# As recommended by Google, googletest is fetched instead of finding from
# local computer
include(ExternalProject)

ExternalProject_Add(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG ac34e6c950925df7165e626becd3f9d64dcd584b
  INSTALL_COMMAND ""
  LOG_DOWNLOAD ON
  LOG_CONFIGURE ON
  LOG_BUILD ON)

ExternalProject_Get_Property(googletest SOURCE_DIR)
set(GMOCK_INCLUDE_DIR
  ${SOURCE_DIR}/googletest/include ${SOURCE_DIR}/googlemock/include)

ExternalProject_Get_Property(googletest BINARY_DIR)

set(GMOCK_LIBRARY gmock)
set(GMOCK_LIBRARY_PATH ${BINARY_DIR}/googlemock/${CMAKE_STATIC_LIBRARY_PREFIX}gmock${CMAKE_STATIC_LIBRARY_SUFFIX})
add_library(${GMOCK_LIBRARY} UNKNOWN IMPORTED)
set_property(TARGET ${GMOCK_LIBRARY} PROPERTY IMPORTED_LOCATION
  ${GMOCK_LIBRARY_PATH} )
add_dependencies(${GMOCK_LIBRARY} googletest)

set(GTEST_LIBRARY gtest)
set(GTEST_LIBRARY_PATH ${BINARY_DIR}/googlemock/gtest/${CMAKE_STATIC_LIBRARY_PREFIX}gtest${CMAKE_STATIC_LIBRARY_SUFFIX})
add_library(${GTEST_LIBRARY} UNKNOWN IMPORTED)
set_property(TARGET ${GTEST_LIBRARY} PROPERTY IMPORTED_LOCATION
  ${GTEST_LIBRARY_PATH} )
add_dependencies(${GTEST_LIBRARY} googletest)

set(BRIDGE_TEST_INCLUDE_DIRS
  ${PROJECT_SOURCE_DIR}/test/include ${GMOCK_INCLUDE_DIR})

add_library(test-common STATIC TestMain.cc ${PROJECT_SOURCE_DIR}/src/Logging.cc ${PROJECT_SOURCE_DIR}/src/Thread.cc)
target_include_directories(test-common PUBLIC ${BRIDGE_TEST_INCLUDE_DIRS})
target_link_libraries(test-common PUBLIC ${GMOCK_LIBRARY} ${GTEST_LIBRARY})
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
add_subdirectory(scoring)
add_subdirectory(engine)
add_subdirectory(main)
