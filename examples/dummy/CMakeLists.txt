cmake_minimum_required(VERSION 3.10)
project(dummy)

if(MAKE_EXECUTABLE)
    add_executable(dummy main.c)
else()
    add_library(dummy SHARED
        main.c
    )
endif()
target_link_libraries(dummy PRIVATE pakrt)
