macro(add_bridge_library target)
  file(GLOB src *.cc)
  add_library(${target} ${src})
  target_compile_features(${target} PUBLIC ${BRIDGE_COMPILE_FEATURES})
endmacro()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

add_subdirectory(main)
add_subdirectory(messaging)
add_subdirectory(cardserver)
add_subdirectory(engine)
add_subdirectory(scoring)
add_subdirectory(bridge)

add_library(bridge-common STATIC Signals.cc Logging.cc Boilerplate.cc)
target_compile_features(bridge-common PUBLIC ${BRIDGE_COMPILE_FEATURES})

add_executable(bridge Bridge.cc)
target_link_libraries(bridge bridge-main bridge-messaging bridge-cardserver
  bridge-engine bridge-scoring bridge-bridge bridge-common ${ZEROMQ_LIBRARIES}
  Threads::Threads)

install(TARGETS bridge RUNTIME DESTINATION bin)

if(LibTMCG_FOUND)
  set(BRIDGE_BUILD_CARD_SERVER ON CACHE BOOL "Build card server")
  if (${BRIDGE_BUILD_CARD_SERVER})
    include_directories(${LibTMCG_INCLUDE_DIR})
    add_subdirectory(csmain)
    add_executable(bridgecs CardServer.cc)
    target_link_libraries(bridgecs bridge-csmain bridge-messaging
      bridge-cardserver bridge-bridge bridge-common ${ZEROMQ_LIBRARIES}
      ${TMCG_LIBRARIES} Threads::Threads)
    install(TARGETS bridgecs RUNTIME DESTINATION bin)
  endif()
endif()
