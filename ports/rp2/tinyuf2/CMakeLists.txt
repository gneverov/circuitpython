cmake_minimum_required(VERSION 3.17)

find_package(Git)

add_library(tinyuf2 INTERFACE)

target_sources(tinyuf2 INTERFACE
    ${TINYUF2_DIR}/src/ghostfat.c
    tinyuf2.c
)

target_include_directories(tinyuf2 INTERFACE
    ${TINYUF2_DIR}/src
    .
)

execute_process(COMMAND ${GIT_EXECUTABLE} describe --dirty --always --tags OUTPUT_VARIABLE GIT_VERSION)
string(STRIP ${GIT_VERSION} GIT_VERSION)
target_compile_definitions(tinyuf2 INTERFACE
    UF2_VERSION="${GIT_VERSION}"
)

target_link_libraries(tinyuf2 INTERFACE
    newlib_helper
)
