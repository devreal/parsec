#include(RulesJDF)
include(ParsecCompilePTG)

include_directories(BEFORE "${CMAKE_CURRENT_BINARY_DIR}")

set(sources
    ${CMAKE_CURRENT_LIST_DIR}/redistribute_wrapper.c
    ${CMAKE_CURRENT_LIST_DIR}/redistribute_dtd.c)

target_sources(parsec PRIVATE ${sources})

set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/redistribute.jdf"
                    "${CMAKE_CURRENT_SOURCE_DIR}/redistribute_reshuffle.jdf"
             APPEND PROPERTY PTGPP_COMPILE_OPTIONS "--Wremoteref")
# Some versions of the XLC compiler generate incorrect aliasing code for the PTG generated code,
# where we cast a parsec_task_t structure into more specialized task classes.
# Restricting the optimizations to avoid the pointer aliasing solves this problem.
string(REGEX MATCH "XL" _match_xlc ${CMAKE_C_COMPILER_ID})
if( _match_xlc )
  set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/redistribute.jdf"
                      "${CMAKE_CURRENT_SOURCE_DIR}/redistribute_reshuffle.jdf"
               APPEND PROPERTY COMPILE_OPTIONS -qalias=noansi)
endif( )

target_ptg_sources(parsec PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/redistribute.jdf;${CMAKE_CURRENT_SOURCE_DIR}/redistribute_reshuffle.jdf")


install(FILES
  redistribute_internal.h
  DESTINATION include/parsec/data_dist/matrix/redistribute )
