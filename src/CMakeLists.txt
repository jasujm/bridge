macro(add_bridge_library target)
  file(GLOB src *.cc)
  add_library(${target} ${src})
  set_property(TARGET ${target} PROPERTY CXX_STANDARD ${BRIDGE_CXX_STANDARD})
  target_include_directories(${target} PUBLIC ${BRIDGE_INCLUDE_DIR})
  target_link_libraries(${target} PRIVATE Boost::boost)
endmacro()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

add_subdirectory(main)
add_subdirectory(messaging)
add_subdirectory(cardserver)
add_subdirectory(engine)
add_subdirectory(bridge)

add_library(bridge-common STATIC Logging.cc Boilerplate.cc Thread.cc)
set_property(TARGET bridge-common PROPERTY CXX_STANDARD ${BRIDGE_CXX_STANDARD})
target_include_directories(bridge-common PUBLIC ${BRIDGE_INCLUDE_DIR})

add_executable(bridge Bridge.cc)
target_link_libraries(bridge bridge-main bridge-messaging bridge-cardserver
  bridge-engine bridge-bridge bridge-common Threads::Threads)
set_property(TARGET bridge PROPERTY CXX_STANDARD ${BRIDGE_CXX_STANDARD})

install(TARGETS bridge RUNTIME DESTINATION bin)

option(BRIDGE_BUILD_CARD_SERVER "Build card server" ON)
if (BRIDGE_BUILD_CARD_SERVER AND LibTMCG_FOUND AND Boost_COROUTINE_FOUND)
  add_subdirectory(coroutines)
  add_subdirectory(csmain)
  add_executable(bridgecs CardServer.cc)
  target_link_libraries(bridgecs bridge-csmain bridge-coroutines
    bridge-messaging bridge-cardserver bridge-bridge bridge-common
    Threads::Threads)
  set_property(TARGET bridgecs PROPERTY CXX_STANDARD ${BRIDGE_CXX_STANDARD})
  install(TARGETS bridgecs RUNTIME DESTINATION bin)
endif()
