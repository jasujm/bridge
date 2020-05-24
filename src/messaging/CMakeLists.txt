add_bridge_library(bridge-messaging)
target_link_libraries(bridge-messaging PUBLIC cppzmq nlohmann_json::nlohmann_json bridge-bridge)
