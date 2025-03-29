cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRCS "dmx4esp.c"
    INCLUDE_DIRS "../include"
    REQUIRES driver freertos
)