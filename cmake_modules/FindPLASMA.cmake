# - Find PLASMA library
# This module finds an installed  library that implements the PLASMA
# linear-algebra interface (see http://icl.cs.utk.edu/plasma/).
# The list of libraries searched for is taken
# from the autoconf macro file, acx_blas.m4 (distributed at
# http://ac-archive.sourceforge.net/ac-archive/acx_blas.html).
#
# This module sets the following variables:
#  PLASMA_FOUND - set to true if a library implementing the PLASMA interface
#    is found
#  PLASMA_LINKER_FLAGS - uncached list of required linker flags (excluding -l
#    and -L).
#  PLASMA_LIBRARIES - uncached list of libraries (using full path name) to
#    link against to use PLASMA
#  PLASMA_STATIC  if set on this determines what kind of linkage we do (static)
#  PLASMA_VENDOR  if set checks only the specified vendor, if not set checks
#     all the possibilities
##########

get_property(_LANGUAGES_ GLOBAL PROPERTY ENABLED_LANGUAGES)
if(NOT _LANGUAGES_ MATCHES Fortran)
  if(PLASMA_FIND_REQUIRED)
    message(FATAL_ERROR "Find PLASMA requires Fortran support so Fortran must be enabled.")
  else(PLASMA_FIND_REQUIRED)
    message(STATUS "Looking for PLASMA... - NOT found (Fortran not enabled)") #
    return()
  endif(PLASMA_FIND_REQUIRED)
endif(NOT _LANGUAGES_ MATCHES Fortran)

set(PLASMA_INCLUDE_DIR)
set(PLASMA_LIBRARIES)
set(PLASMA_LINKER_FLAGS)

if(PLASMA_FIND_QUIETLY OR NOT PLASMA_FIND_REQUIRED)
  find_package(LAPACK)
else(PLASMA_FIND_QUIETLY OR NOT PLASMA_FIND_REQUIRED)
  find_package(LAPACK REQUIRED)
endif(PLASMA_FIND_QUIETLY OR NOT PLASMA_FIND_REQUIRED)
#message("Found BLAS library in ${BLAS_LIBRARIES}")
#message("Found LAPACK library in ${LAPACK_LIBRARIES}")

include(CheckFortranFunctionExists)
include(CheckIncludeFile)

if(LAPACK_FOUND)
  set(CMAKE_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES};${PLASMA_INCLUDE_DIR}")
  #  message(STATUS "Looking for plasma.h in ${PLASMA_INCLUDE_DIR}")
  check_include_file(plasma.h FOUND_PLASMA_INCLUDE)
  if(FOUND_PLASMA_INCLUDE)
    find_library(PLASMA_cblas_LIB cblas
      PATHS ${PLASMA_LIBRARIES}
      DOC "Where the PLASMA cblas libraries are"
      NO_DEFAULT_PATH)
    if( NOT PLASMA_cblas_LIB )
      find_library(PLASMA_cblas_LIB cblas
        PATHS ${PLASMA_LIBRARIES}
        DOC "Where the PLASMA cblas libraries are")
    endif( NOT PLASMA_cblas_LIB )
    find_library(PLASMA_coreblas_LIB coreblas
      PATHS ${PLASMA_LIBRARIES}
      DOC "Where the PLASMA coreblas libraries are")
    find_library(PLASMA_corelapack_LIB corelapack
      PATHS ${PLASMA_LIBRARIES}
      DOC "Where the PLASMA corelapack libraries are")
    find_library(PLASMA_plasma_LIB plasma
      PATHS ${PLASMA_LIBRARIES}
      DOC "Where the PLASMA plasma libraries are")
    if( PLASMA_cblas_LIB AND PLASMA_coreblas_LIB AND PLASMA_corelapack_LIB AND PLASMA_plasma_LIB )
      set( PLASMA_LIBRARIES "${PLASMA_plasma_LIB};${PLASMA_coreblas_LIB};${PLASMA_corelapack_LIB};${PLASMA_cblas_LIB}")
      set( FOUND_PLASMA_LIB 1)
    endif( PLASMA_cblas_LIB AND PLASMA_coreblas_LIB AND PLASMA_corelapack_LIB AND PLASMA_plasma_LIB )
  endif(FOUND_PLASMA_INCLUDE)
  
  if(FOUND_PLASMA_INCLUDE AND FOUND_PLASMA_LIB)
    set(PLASMA_FOUND TRUE)
  else(FOUND_PLASMA_INCLUDE AND FOUND_PLASMA_LIB)
    set(PLASMA_FOUND FALSE)
  endif(FOUND_PLASMA_INCLUDE AND FOUND_PLASMA_LIB)
endif(LAPACK_FOUND)

include(FindPackageMessage)
find_package_message(PLASMA "Found PLASMA: ${PLASMA_LIBRARIES}"
  "[${PLASMA_INCLUDE_DIR}][${PLASMA_LIBRARIES}]")

if(NOT PLASMA_FIND_QUIETLY)
  if(PLASMA_FOUND)
    message(STATUS "A library with PLASMA API found.")
  else(PLASMA_FOUND)
    if(PLASMA_FIND_REQUIRED)
      message(FATAL_ERROR
        "A required library with PLASMA API not found. Please specify library location."
        )
    else(PLASMA_FIND_REQUIRED)
      message(STATUS
        "A library with PLASMA API not found. Please specify library location."
        )
    endif(PLASMA_FIND_REQUIRED)
  endif(PLASMA_FOUND)
endif(NOT PLASMA_FIND_QUIETLY)
