find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_GLM FFTW3f QUIET)
endif()

find_path(FFTW3_INCLUDE_DIR fftw3.h
                          PATHS ${PC_GLM_INCLUDEDIR}
                          PATH_SUFFIXES glm)

FIND_LIBRARY(FFTW3_LIBS NAMES fftw3 fftw3f fftw3l
             PATHS ${PC_SQLITE3_INCLUDEDIR}
             PATH_SUFFIXES lib
             )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFTW3f REQUIRED_VARS GLM_INCLUDE_DIR)

mark_as_advanced(GLM_INCLUDE_DIR)
