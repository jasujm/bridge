file(GLOB bridgegui_files *.py)
add_custom_target(
  bridge-bridgegui ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${bridgegui_files} ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_LIST_DIR}/images ${CMAKE_CURRENT_BINARY_DIR}/images
  DEPENDS ${bridgegui_files})
