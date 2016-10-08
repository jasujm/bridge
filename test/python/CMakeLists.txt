if (${BRIDGE_BUILD_CARD_SERVER} AND ${PYTHONINTERP_FOUND} AND NOT
    ${PYTHON_VERSION_MAJOR} LESS 3)
  file(COPY test_card_server.py DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_target(
    test-card-server ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_CURRENT_LIST_DIR}/test_card_server.py test_card_server.py
    DEPENDS test_card_server.py)
  add_test(
    NAME TestCardServer
    COMMAND ${PYTHON_EXECUTABLE} test_card_server.py $<TARGET_FILE:bridge-cs>)
endif()
