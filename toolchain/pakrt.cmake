if(NOT TARGET pakrt)
    add_library(pakrt INTERFACE IMPORTED)
    target_include_directories(pakrt INTERFACE "${CMAKE_CURRENT_LIST_DIR}/../include")
endif()
