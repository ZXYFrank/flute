file(GLOB SOURCES_HEADERS ./*.cc ./*.h)
add_library(flute_common ${SOURCES_HEADERS})
target_link_libraries(flute_common pthread rt)
# unit tests
add_subdirectory(tests)