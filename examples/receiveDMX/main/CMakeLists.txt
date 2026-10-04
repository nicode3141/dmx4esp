cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../src")

idf_component_register(SRCS "dmxRead.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver freertos src)
