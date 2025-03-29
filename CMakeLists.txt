cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "./tests")

set(COMPONENTS_DIRS "${CMAKE_SOURCE_DIR}/src")



include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(dmx4esp)

idf_component_register(SRCS "src/esp4dmx.c" INCLUDE_DIRS "include")