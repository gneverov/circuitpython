cmake_minimum_required(VERSION 3.13)

add_library(fatfs INTERFACE)

target_sources(fatfs INTERFACE
    source/ff.c
    source/ffunicode.c
)

target_include_directories(fatfs INTERFACE source)
