cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED ENV{IDF_PATH})
    macro(idf_component_register)
        # Do nothing, just prevent CMake configuration from failing
    endmacro()
endif()
idf_component_register(
    SRCS "dmx4esp.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos
)