set(BRIDGE_BUILD_TESTS "Build tests using googletest framework" CACHE BOOL OFF)

if (BRIDGE_BUILD_TESTS)
  # As recommended by Google, googletest is fetched instead of finding from
  # local computer
  include(ExternalProject)
  function(add_googlemock_library library)
    add_library(${library} IMPORTED STATIC)
    add_dependencies(${library} googletest)
    ExternalProject_Get_Property(googletest BINARY_DIR)
    set_target_properties(${library}
      PROPERTIES IMPORTED_LOCATION
      ${BINARY_DIR}/googlemock/${CMAKE_STATIC_LIBRARY_PREFIX}${library}${CMAKE_STATIC_LIBRARY_SUFFIX})
  endfunction()

  ExternalProject_Add(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG ec44c6c1675c25b9827aacd08c02433cccde7780
    INSTALL_COMMAND "")
  add_googlemock_library(gmock)
  add_googlemock_library(gmock_main)
  ExternalProject_Get_Property(googletest SOURCE_DIR)
  set(GOOGLETEST_INCLUDE_DIR
    ${SOURCE_DIR}/googletest/include ${SOURCE_DIR}/googlemock/include)

  set(BRIDGE_TEST_INCLUDE_DIRS
    ${PROJECT_SOURCE_DIR}/test/include ${GOOGLETEST_INCLUDE_DIR})

  function(add_bridge_test)
    set(OneValueArgs TARGET NAME)
    set(MultiValueArgs DEPENDS)
    cmake_parse_arguments(BRIDGE_TEST
      "" "${OneValueArgs}" "${MultiValueArgs}" ${ARGN})
    file(GLOB src *.cc)
    add_executable(${BRIDGE_TEST_TARGET} ${src})
    target_include_directories(${BRIDGE_TEST_TARGET}
      PRIVATE ${BRIDGE_TEST_INCLUDE_DIRS})
    target_link_libraries(${BRIDGE_TEST_TARGET} ${BRIDGE_TEST_DEPENDS}
      gmock gmock_main Threads::Threads)
    add_test(${BRIDGE_TEST_NAME} ${BRIDGE_TEST_TARGET})
  endfunction()

  add_subdirectory(bridge)
  add_subdirectory(messaging)
  add_subdirectory(scoring)
  add_subdirectory(engine)
  add_subdirectory(main)

endif()
