set(BRIDGE_BUILD_TESTS ON CACHE BOOL "Build tests using googletest framework")

if (BRIDGE_BUILD_TESTS)
  add_subdirectory(src)
  add_subdirectory(python)
endif()
