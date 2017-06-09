cmake_minimum_required(VERSION 3.7)
project(zmg_utils)

set(CMAKE_C_STANDARD 99)

set(COMMON_SOURCE
        zmgfs.h zmgfs.c
        utarray.h uthash.h utlist.h)

add_executable(makezmg ${COMMON_SOURCE} makezmg.c)
add_executable(zmgsh ${COMMON_SOURCE} zmgsh.c)
add_executable(zmgmnt ${COMMON_SOURCE} zmgmnt.c)
target_compile_definitions(zmgmnt
        PRIVATE -D_FILE_OFFSET_BITS=64)
target_link_libraries(makezmg z)
target_link_libraries(zmgsh z)
target_link_libraries(zmgmnt z fuse pthread)