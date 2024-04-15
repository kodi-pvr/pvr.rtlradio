find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_LIBFMT fmt QUIET)
endif()

find_path(LIBFMT_INCLUDE_DIRS format.h
                              PATHS ${PC_LIBFMT_INCLUDEDIR}
                              PATH_SUFFIXES fmt)
find_library(LIBFMT_LIBRARIES NAMES fmt fmtd
                              PATHS ${PC_LIBFMT_LIBDIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBFMT REQUIRED_VARS LIBFMT_LIBRARIES LIBFMT_INCLUDE_DIRS)

mark_as_advanced(LIBFMT_INCLUDE_DIRS LIBFMT_LIBRARIES)
