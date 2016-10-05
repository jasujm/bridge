set(BRIDGE_BUILD_TESTS "Build tests using googletest framework" CACHE BOOL OFF)

if (BRIDGE_BUILD_TESTS)
  add_subdirectory(src)
endif()
