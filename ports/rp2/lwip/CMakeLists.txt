cmake_minimum_required(VERSION 3.13)

add_library(lwip_helper INTERFACE)

target_sources(lwip_helper INTERFACE
    lwip_init.c
)

target_include_directories(lwip_helper INTERFACE include)