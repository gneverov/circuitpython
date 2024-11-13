cmake_minimum_required(VERSION 3.13)

set(LITTLEFS_DIR ${MICROPY_DIR}/lib/littlefs)

add_library(littlefs INTERFACE)

target_sources(littlefs INTERFACE
    ${LITTLEFS_DIR}/lfs.c
    ${LITTLEFS_DIR}/lfs_util.c
)

target_include_directories(littlefs INTERFACE ${LITTLEFS_DIR})

target_compile_definitions(littlefs INTERFACE
    $<$<NOT:$<CONFIG:DEBUG>>:LFS_NO_ASSERT>
)
