include(ParsecCompilePTG)

parsec_addtest_executable(C schedmicro SOURCES main.c ep_wrapper.c schedmicro_data.c)
target_ptg_sources(schedmicro PRIVATE "ep.jdf")
target_link_libraries(schedmicro PRIVATE m)

