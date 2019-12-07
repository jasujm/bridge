add_bridge_library(bridge-main)
target_include_directories(bridge-main PRIVATE ${LUA_INCLUDE_DIR})
target_link_libraries(bridge-main PRIVATE ${LUA_LIBRARIES} bridge-bridge)
