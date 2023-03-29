file(GLOB SOURCES_HEADERS ./*.cc ./*.h)
add_library(flute_net_http ${SOURCES_HEADERS})
target_link_libraries(flute_net_http pthread rt)
target_link_libraries(flute_net_http flute_net)

add_subdirectory(tests)