cmake_minimum_required(VERSION 3.27)

project(picolibc LANGUAGES NONE)

set(ARCH thumbv6m-none-eabi)

set(SOURCE_DIR ../../../lib/picolibc)
cmake_path(ABSOLUTE_PATH SOURCE_DIR NORMALIZE)
set(BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/build)
set(INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/install)

find_program(MESON meson)

add_custom_command(
    OUTPUT ${BUILD_DIR}/build.ninja
    COMMAND 
        ${MESON} 
        setup
        --reconfigure
        --cross-file=${CMAKE_CURRENT_LIST_DIR}/cross-${ARCH}.txt
        --buildtype minsize
        -Dprefix=${INSTALL_DIR}
        -Dspecsdir=none
        -Dtests=false
        -Dmultilib=false
        -Dposix-console=true
        -Dsemihost=false
        -Dpicolib=true
        -Dpicocrt=false
        -Dnewlib-initfini=false
        -Dcrt-runtime-size=true
        -Dthread-local-storage=true
        -Dnewlib-iconv-encodings=us_ascii,utf_8,utf_16
        ${BUILD_DIR}
        ${SOURCE_DIR}
    VERBATIM
)

find_program(NINJA ninja)

add_custom_command(
    OUTPUT ${BUILD_DIR}/newlib/libc.a
    COMMAND ${NINJA} -C ${BUILD_DIR}
    DEPENDS ${BUILD_DIR}/build.ninja
    VERBATIM
)

add_custom_command(
    OUTPUT ${INSTALL_DIR}/lib/libc.a
    COMMAND ${NINJA} -C ${BUILD_DIR} install
    DEPENDS ${BUILD_DIR}/newlib/libc.a
    VERBATIM    
)

add_custom_target(picolibc ALL
    DEPENDS ${INSTALL_DIR}/lib/libc.a
)

# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem ${INSTALL_DIR}/include -ftls-model=local-exec" PARENT_SCOPE)
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${INSTALL_DIR}/lib" PARENT_SCOPE)

add_custom_target(picolibc_wipe
    COMMAND 
        ${MESON}
        setup
        --wipe
        ${BUILD_DIR}
        ${SOURCE_DIR}
    VERBATIM 
)
