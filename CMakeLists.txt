cmake_minimum_required(VERSION 3.16)

#run tests when build in project folder
#if you just want to use the esp4dmx library, move dmx4esp/esp4dmx into your components folder
#set(EXTRA_COMPONENT_DIRS "./dmx4esp")
set(EXTRA_COMPONENT_DIRS "./tests")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(dmx4esptest)