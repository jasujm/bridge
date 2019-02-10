add_bridge_library(bridge-coroutines)
target_include_directories(bridge-coroutines PUBLIC ${ZEROMQ_INCLUDE_DIRS})
target_link_libraries(bridge-coroutines PUBLIC ${ZEROMQ_LIBRARIES}
  Boost::coroutine)
