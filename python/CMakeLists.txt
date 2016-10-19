set(BRIDGE_BUILD_GUI ON CACHE BOOL "Build GUI")
if (${BRIDGE_BUILD_GUI})
  file(GLOB_RECURSE python_files bridgegui/*.py)
  file(GLOB_RECURSE image_files bridgegui/images/*.png)
  configure_file(setup.py.in setup.py)
  add_custom_target(
    bridge-bridgegui ALL
    COMMAND ${PYTHON_EXECUTABLE} setup.py build
    DEPENDS ${python_files} ${image_files})
  add_test(
    NAME TestBridgeGui
    COMMAND ${PYTHON_EXECUTABLE} setup.py test)
  install(
    CODE "execute_process(
      COMMAND ${PYTHON_EXECUTABLE} setup.py install
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})")
endif()
