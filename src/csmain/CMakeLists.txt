add_bridge_library(bridge-csmain)
target_include_directories(bridge-csmain PRIVATE ${LibTMCG_INCLUDE_DIR})
target_link_libraries(bridge-csmain PUBLIC bridge-bridge)
