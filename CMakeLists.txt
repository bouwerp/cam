cmake_minimum_required(VERSION 3.14)
project(cam C)

set(CMAKE_C_STANDARD 11)

# location to include files
include_directories("include")

# the actual library
add_library(cam cam.c)
