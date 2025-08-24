cmake_minimum_required(VERSION 3.22.1)

project("nativelibrary")

add_library(
    nativelibrary
    SHARED
    native-lib.cpp
    command.h
)

find_library(
    log-lib
    log
)

target_link_libraries(
    nativelibrary
    ${log-lib}
)