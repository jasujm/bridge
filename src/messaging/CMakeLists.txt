add_bridge_library(bridge-messaging)
target_include_directories(bridge-messaging PUBLIC ${ZEROMQ_INCLUDE_DIRS} ${JSON_INCLUDE_DIRS})
target_link_libraries(bridge-messaging PUBLIC ${ZEROMQ_LIBRARIES} bridge-bridge)
