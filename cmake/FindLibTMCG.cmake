find_library(TMCG_LIBRARY TMCG)
find_path(TMCG_INCLUDE_DIR libTMCG.hh)

set(TMCG_LIBRARIES ${TMCG_LIBRARY})
set(TMCG_INCLUDE_DIRS ${TMCG_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibTMCG DEFAULT_MSG TMCG_LIBRARY
  TMCG_INCLUDE_DIR)
