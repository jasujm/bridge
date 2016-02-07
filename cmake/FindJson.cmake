find_path(JSON_INCLUDE_DIR json.hpp)

set(JSON_INCLUDE_DIRS ${Json_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Json DEFAULT_MSG JSON_INCLUDE_DIR)
