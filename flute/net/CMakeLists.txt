file(GLOB SOURCES_HEADERS ./*.cc ./*.h)
add_library(flute_net ${SOURCES_HEADERS})
target_link_libraries(flute_net pthread rt)
target_link_libraries(flute_net flute_common)
# unit tests
add_subdirectory(tests)
add_subdirectory(http)
add_subdirectory(poller)