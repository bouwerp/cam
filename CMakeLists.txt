cmake_minimum_required(VERSION 3.14)
project(cam CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

# location to include files
include_directories(
        "../userland"
)

link_directories(
        ../userland/build/lib
)

# the actual library
add_library(cam cam.cc)