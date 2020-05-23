include(FindPython)

if (BRIDGE_BUILD_CARD_SERVER AND LibTMCG_FOUND AND Boost_COROUTINE_FOUND)
  find_package(Python COMPONENTS Interpreter)
  if (Python_FOUND AND NOT ${Python_VERSION_MAJOR} LESS 3)
    add_custom_target(
      test-card-server ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/test_card_server.py ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS test_card_server.py bridgecs)
    add_test(
      NAME TestCardServer
      COMMAND Python::Interpreter test_card_server.py $<TARGET_FILE:bridgecs>)
  endif()
endif()
