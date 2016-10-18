file(GLOB bridgegui_files *.py)
add_custom_target(
  test-bridgegui ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${bridgegui_files} ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${bridgegui_files} bridge-bridgegui)
add_test(
  NAME TestBridgeGui
  COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${BRIDGE_PYTHON_DIR}
    ${PYTHON_EXECUTABLE} -m unittest -v)
