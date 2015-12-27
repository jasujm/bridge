set(BRIDGE_LIBRARIES "" PARENT_SCOPE)

macro(add_bridge_library target)
  file(GLOB src *.cc)
  add_library(${target} ${src})
  target_compile_features(${target} PUBLIC ${BRIDGE_COMPILE_FEATURES})
  set(BRIDGE_LIBRARIES ${BRIDGE_LIBRARIES} ${target} PARENT_SCOPE)
endmacro()

add_subdirectory(main)
add_subdirectory(engine)
add_subdirectory(scoring)
add_subdirectory(bridge)

set(BRIDGE_LIBRARIES ${BRIDGE_LIBRARIES} PARENT_SCOPE)
