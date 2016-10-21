if (${BRIDGE_BUILD_CARD_SERVER})
  add_custom_target(
    test-card-server ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_CURRENT_SOURCE_DIR}/test_card_server.py ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS test_card_server.py bridgecs)
  add_test(
    NAME TestCardServer
    COMMAND ${PYTHON_EXECUTABLE} test_card_server.py $<TARGET_FILE:bridgecs>)
endif()
