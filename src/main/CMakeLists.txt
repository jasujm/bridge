add_bridge_library(bridge-main)

option(BRIDGE_BUILD_RECORDER "Include bridge game recorder" ON)
if (RocksDB_FOUND AND BRIDGE_BUILD_RECORDER)
  target_compile_definitions(bridge-main PUBLIC WITH_RECORDER=1)
  set(BRIDGE_MAIN_ADDITIONAL_LIBRARIES RocksDB::rocksdb-shared)
endif()

target_include_directories(bridge-main PRIVATE ${LUA_INCLUDE_DIR})
target_link_libraries(
  bridge-main PRIVATE ${LUA_LIBRARIES} ${BRIDGE_MAIN_ADDITIONAL_LIBRARIES}
  bridge-bridge)
