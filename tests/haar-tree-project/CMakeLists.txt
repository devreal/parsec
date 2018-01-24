include(RulesJDF)

jdf_rules(project_generated "${CMAKE_CURRENT_SOURCE_DIR}/project.jdf;${CMAKE_CURRENT_SOURCE_DIR}/walk.jdf")
jdf_rules(project_dyn_generated "${CMAKE_CURRENT_SOURCE_DIR}/project_dyn.jdf;${CMAKE_CURRENT_SOURCE_DIR}/walk.jdf")
jdf_rules(rwalk_generated "${CMAKE_CURRENT_SOURCE_DIR}/random_walk.jdf")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

parsec_addtest(C project "main.c;${project_generated};tree_dist.c")
parsec_addtest(C rwalk "rand_walk.c;${rwalk_generated}")
parsec_addtest(C irtest "ir_test.c;${rwalk_generated};${project_dyn_generated};osu_allreduce.c;osu_coll.c;tree_dist.c")
parsec_addtest(C project_dyn "main.c;${project_dyn_generated};tree_dist.c")
target_compile_definitions(project_dyn PUBLIC parsec_project_new=parsec_project_dyn_new)
target_compile_definitions(irtest PUBLIC parsec_project_new=parsec_project_dyn_new)

add_test(unit_haar_tree_shm ${SHM_TEST_CMD_LIST} ./project -x)
if( MPI_C_FOUND )
  add_test(unit_haar_tree_mpi ${MPI_TEST_CMD_LIST} 4 ./project -x)
  set_tests_properties(unit_haar_tree_mpi PROPERTIES DEPENDS launch_mpi)
endif( MPI_C_FOUND )

