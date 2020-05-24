# Modeled after the example in the official docs:
# https://cmake.org/cmake/help/v3.11/manual/cmake-developer.7.html

find_path(LIBTMCG_INCLUDE_DIR
  NAMES libTMCG.hh)

find_library(LIBTMCG_LIBRARY
  NAMES TMCG)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibTMCG
  FOUND_VAR LIBTMCG_FOUND
  REQUIRED_VARS LIBTMCG_LIBRARY LIBTMCG_INCLUDE_DIR)

if (LIBTMCG_FOUND)
  set(LIBTMCG_LIBRARIES ${LIBTMCG_LIBRARY})
  set(LIBTMCG_INCLUDE_DIRS ${LIBTMCG_INCLUDE_DIR})
endif()

if (LIBTMCG_FOUND AND NOT TARGET LibTMCG::LibTMCG)
  add_library(LibTMCG::LibTMCG UNKNOWN IMPORTED)
  set_target_properties(LibTMCG::LibTMCG PROPERTIES
    IMPORTED_LOCATION "${LIBTMCG_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBTMCG_INCLUDE_DIR}")
endif()

mark_as_advanced(LIBTMCG_LIBRARY LIBTMCG_INCLUDE_DIR)
