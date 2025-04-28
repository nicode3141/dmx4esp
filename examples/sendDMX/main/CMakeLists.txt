cmake_minimum_required(VERSION 3.16)

idf_component_register(SRCS "dmxSend.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver freertos dmx4esp)
