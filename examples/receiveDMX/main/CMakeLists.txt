cmake_minimum_required(VERSION 3.16)

idf_component_register(SRCS "rgbLed.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver freertos dmx4esp)
