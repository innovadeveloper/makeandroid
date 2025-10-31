cmake_minimum_required(VERSION 3.22.1)

project("nativelibrary2")

add_library(
    nativelibrary2
    SHARED
    native-lib.cpp
    thread_helper.c
)

find_library(
    log-lib
    log
)

target_link_libraries(
    nativelibrary2
    ${log-lib}
)