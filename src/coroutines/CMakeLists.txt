add_bridge_library(bridge-coroutines)
target_link_libraries(bridge-coroutines PUBLIC cppzmq Boost::coroutine)
