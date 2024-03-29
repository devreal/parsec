# Add a blanket definition on everything that's built during the build stage.
# We do not use target_compile_definitions to avoid having to track all object
# libraries that could miss this otherwise.
add_compile_definitions(BUILDING_PARSEC)
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Fortran>>:${PARSEC_ATOMIC_SUPPORT_OPTIONS}>")
#
#  Settings for targets
#
set(BASE_SOURCES
  class/parsec_dequeue.c
  class/parsec_fifo.c
  class/parsec_lifo.c
  class/parsec_list.c
  class/parsec_object.c
  class/parsec_value_array.c
  class/parsec_hash_table.c
  class/parsec_rwlock.c
  class/parsec_future.c
  class/parsec_datacopy_future.c
  class/info.c
  class/parsec_future.c
  class/parsec_datacopy_future.c
  utils/argv.c
  utils/cmd_line.c
  utils/colors.c
  utils/parsec_environ.c
  utils/installdirs.c
  utils/keyval_parse.c
  utils/mca_param.c
  utils/mca_param_cmd_line.c
  utils/mca_parse_paramfile.c
  utils/os_path.c
  utils/output.c
  utils/show_help.c
  utils/zone_malloc.c
  utils/atomic_external.c
  utils/debug.c
  utils/win_compat.c
)

FLEX_TARGET(show_help_flex utils/show_help_lex.l ${CMAKE_CURRENT_BINARY_DIR}/show_help_lex.l.c)
list(APPEND BASE_SOURCES ${FLEX_show_help_flex_OUTPUTS})

FLEX_TARGET(keyval_flex utils/keyval_lex.l ${CMAKE_CURRENT_BINARY_DIR}/keyval_lex.l.c)
list(APPEND BASE_SOURCES ${FLEX_keyval_flex_OUTPUTS})

if(PARSEC_FLEX_GENERATED_OPTIONS)
  set_source_files_properties(${FLEX_show_help_flex_OUTPUTS}
    PROPERTIES COMPILE_OPTIONS "${PARSEC_FLEX_GENERATED_OPTIONS}")
  set_source_files_properties(${FLEX_keyval_flex_OUTPUTS}
    PROPERTIES COMPILE_OPTIONS "${PARSEC_FLEX_GENERATED_OPTIONS}")
endif(PARSEC_FLEX_GENERATED_OPTIONS)

# Read the Modular Components
#   This must be read using include and not add_subdirectory because
#   we want the mca/CMakeLists.txt to export the MCA_EXTRA_SOURCES it builds.
include(mca/CMakeLists.txt)

# Import all the available Domain Specific Language interfaces
include(interfaces/CMakeLists.txt)
add_executable(PaRSEC::parsec-ptgpp ALIAS parsec-ptgpp)

if( PARSEC_PAPI_SDE )
  list(APPEND EXTRA_SOURCES "papi_sde.c")
endif( PARSEC_PAPI_SDE )

# find_package(TAU REQUIRED)
# include_directories(${TAU_INCLUDE_DIRS})
# message(WARNING ${TAU_INCLUDE_DIRS})
# list(APPEND EXTRA_LIBS ${TAU_LIBRARIES})
# set (CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib" ${TAU_LIBRARY_DIR})

# Assume no support for OTF2
set(PARSEC_HAVE_OTF2 FALSE CACHE INTERNAL "Has support for the OTF2 format")
if(PARSEC_PROF_TRACE)
  if(PARSEC_PROF_TRACE_SYSTEM STREQUAL "OTF2" OR PARSEC_PROF_TRACE_SYSTEM STREQUAL "Auto")
    find_package(OTF2 2.1.1)
    if(OTF2_FOUND AND MPI_C_FOUND)
      list(APPEND EXTRA_SOURCES "profiling_otf2.c")
      list(APPEND EXTRA_LIBS "otf2")
      MESSAGE(STATUS "Profiling uses OTF2")
      set(PARSEC_HAVE_OTF2 ${OTF2_FOUND})
    else(OTF2_FOUND AND MPI_C_FOUND)
      if(PARSEC_PROF_TRACE_SYSTEM STREQUAL "OTF2")
        MESSAGE(FATAL_ERROR "Requested OTF2 tracing system is not available because OTF2 was not found or MPI was not found")
      else(PARSEC_PROF_TRACE_SYSTEM STREQUAL "OTF2")
        list(APPEND EXTRA_SOURCES "profiling.c")
        MESSAGE(STATUS "Profiling uses PaRSEC Binary Tracing Format")
      endif(PARSEC_PROF_TRACE_SYSTEM STREQUAL "OTF2")
    endif(OTF2_FOUND AND MPI_C_FOUND)
  else(PARSEC_PROF_TRACE_SYSTEM STREQUAL "OTF2" OR PARSEC_PROF_TRACE_SYSTEM STREQUAL "Auto")
    list(APPEND EXTRA_SOURCES "profiling.c")
    MESSAGE(STATUS "Profiling uses PaRSEC Binary Tracing Format")
  endif(PARSEC_PROF_TRACE_SYSTEM STREQUAL "OTF2" OR PARSEC_PROF_TRACE_SYSTEM STREQUAL "Auto")
endif(PARSEC_PROF_TRACE)

set(SOURCES
  arena.c
  barrier.c
  bindthread.c
  parsec.c
  parsec_reshape.c
  data.c
  data_distribution.c
  debug_marks.c
  mca/mca_repository.c
  mempool.c
  private_mempool.c
  remote_dep.c
  scheduling.c
  compound.c
  vpmap.c
  maxheap.c
  hbbuffer.c
  datarepo.c
  ${EXTRA_SOURCES}
  ${MCA_EXTRA_SOURCES}
)
if( PARSEC_PROF_TRACE )
  list(APPEND SOURCES dictionary.c)
endif( PARSEC_PROF_TRACE )
if( NOT MPI_C_FOUND )
  list(APPEND SOURCES datatype/datatype.c)
else( NOT MPI_C_FOUND )
  list(APPEND SOURCES datatype/datatype_mpi.c)
endif( NOT MPI_C_FOUND )
list(APPEND SOURCES parsec_hwloc.c)

if( PARSEC_PROF_GRAPHER )
  list(APPEND SOURCES
      parsec_prof_grapher.c)
endif( PARSEC_PROF_GRAPHER )

#
# Setup targets
#
if( BUILD_PARSEC )
  # Build this as a collection of objects that can be easily either used to
  # create an interface library or imported into the main library.
  if(NOT TARGET parsec-base-obj)
    add_library(parsec-base-obj OBJECT ${BASE_SOURCES})
  endif(NOT TARGET parsec-base-obj)
  target_compile_definitions(parsec-base-obj
    PRIVATE
    YYERROR_VERBOSE)
  target_compile_options(parsec-base-obj
    PUBLIC ${PARSEC_ATOMIC_SUPPORT_OPTIONS})
  target_include_directories(parsec-base-obj
    INTERFACE
    $<INSTALL_INTERFACE:${PARSEC_INSTALL_INCLUDEDIR}>
    $<$<NOT:${PARSEC_BUILD_INPLACE}>:
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/parsec/include>>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/parsec/include>)
  target_link_libraries(parsec-base-obj
    PUBLIC
    ${PARSEC_ATOMIC_SUPPORT_LIBS}
    Threads::Threads
    $<$<BOOL:${PARSEC_HAVE_WS2_32}>:ws2_32>)
  set_target_properties(parsec-base-obj PROPERTIES POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})

  if(NOT TARGET parsec-base)
    add_library(parsec-base STATIC)
    target_link_libraries(parsec-base PUBLIC parsec-base-obj)
  endif(NOT TARGET parsec-base)

  # As suggested @ https://crascit.com/2016/01/31/enhanced-source-file-handling-with-target_sources/
  # create the target at the upper level and add sources to all directories below.
  if(NOT TARGET parsec)
    if( ${BUILD_SHARED_LIBS} )
      add_library(parsec SHARED)
    else( ${BUILD_SHARED_LIBS} )
      add_library(parsec STATIC)
    endif( ${BUILD_SHARED_LIBS} )
  endif(NOT TARGET parsec)
  add_library(PaRSEC::parsec ALIAS parsec)

  target_sources(parsec PRIVATE $<IF:$<BOOL:PARSEC_SANITIZE_COMPILE_OPTIONS>,${BASE_SOURCES},$<TARGET_OBJECTS:parsec-base-obj>>)
  target_sources(parsec PRIVATE ${SOURCES})
  add_subdirectory(data_dist)

  target_compile_options(parsec
    PRIVATE
    ${PARSEC_SANITIZE_COMPILE_OPTIONS}
    INTERFACE
    #for tests and examples
    $<BUILD_INTERFACE:${PARSEC_SANITIZE_COMPILE_OPTIONS}>)
  target_link_options(parsec
    PUBLIC
    ${PARSEC_SANITIZE_LINK_OPTIONS})
  target_compile_definitions(parsec
    PRIVATE
    YYERROR_VERBOSE)
  target_include_directories(parsec
    INTERFACE
    $<INSTALL_INTERFACE:${PARSEC_INSTALL_INCLUDEDIR}>
    $<$<NOT:${PARSEC_BUILD_INPLACE}>:
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/parsec/include>>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/parsec/include>)
  target_link_libraries(parsec
    PRIVATE
    Threads::Threads
    $<$<BOOL:${PARSEC_SHM_OPEN_IN_LIBRT}>:rt>
    PUBLIC
    $<$<BOOL:${PARSEC_HAVE_DLFCN_H}>:${CMAKE_DL_LIBS}> # Add support for dlopen if necessary
    $<$<BOOL:${HWLOC_FOUND}>:HWLOC::HWLOC>
    $<$<BOOL:${PARSEC_HAVE_OTF2}>:otf2>
    $<$<BOOL:${MPI_C_FOUND}>:MPI::MPI_C>
    $<$<BOOL:${PARSEC_HAVE_CUDA}>:CUDA::cudart>
    ${EXTRA_LIBS}
    INTERFACE
    ${PARSEC_ATOMIC_SUPPORT_LIBS}
    )

  set_target_properties(parsec PROPERTIES POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
  # For now make sure that the installed shared libraries have the right
  # version (same version as the major of the build version). Thus, every
  # change in the high-level API (and ABI) should be reflected on the
  # major version of PaRSEC.
  set_target_properties(parsec PROPERTIES VERSION ${PARSEC_VERSION_MAJOR}.${PARSEC_VERSION_MINOR}.${PARSEC_VERSION_RELEASE}
    SOVERSION ${PARSEC_VERSION_MAJOR})

  if( PARSEC_PAPI_SDE )
    # This file is in fact provided by PAPI SDE, to serve as a fallback in case PAPI
    # is not selected / loaded. It should be dynamic library that comes after PAPI
    if(NOT TARGET parsec-papi-sde)
      add_library(parsec-papi-sde SHARED papi_sde_interface.c)
    endif(NOT TARGET parsec-papi-sde)
    # parsec-papi-sde must come last in EXTRA_LIBS: it is to be preceded by papi if
    # PAPI is loaded so that the weak symbols in it are overcome by PAPI
    target_link_libraries(parsec
      PUBLIC parsec-papi-sde $<$<BOOL:${PAPI_FOUND}>:PAPI::PAPI>)

    # parsec-papi-sde has to be installed, to provide the default empty functions
    install(TARGETS parsec-papi-sde
      EXPORT parsec-targets
      DESTINATION ${PARSEC_INSTALL_LIBDIR})
  #install(FILES papi_sde_interface.h DESTINATION ${PARSEC_INSTALL_LIBDIR}/parsec)
  endif( PARSEC_PAPI_SDE )

#
# If we have support for F90 build the PaRSEC module
#
  if(CMAKE_Fortran_COMPILER_SUPPORTS_F90)
    add_subdirectory(fortran)
  endif(CMAKE_Fortran_COMPILER_SUPPORTS_F90)

  install(TARGETS parsec
    EXPORT parsec-targets
    ARCHIVE DESTINATION ${PARSEC_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${PARSEC_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${PARSEC_INSTALL_INCLUDEDIR})

  install(FILES utils/help-mca-param.txt DESTINATION ${PARSEC_INSTALL_DATADIR}/parsec)
ENDIF( BUILD_PARSEC )

