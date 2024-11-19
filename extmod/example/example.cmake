add_library(extmod_example INTERFACE)

target_sources(extmod_example INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modexample.c
)

set_source_files_properties(
    ${CMAKE_CURRENT_LIST_DIR}/modexample.c
    PROPERTIES MICROPY_SOURCE_QSTR ON
)
