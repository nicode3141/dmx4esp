# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/nicol/esp/esp-idf/components/bootloader/subproject"
  "C:/Users/nicol/OneDrive/nicodeGmbH/ESP32-Projects/dmx4esp/dmx4espTest/build/bootloader"
  "C:/Users/nicol/OneDrive/nicodeGmbH/ESP32-Projects/dmx4esp/dmx4espTest/build/bootloader-prefix"
  "C:/Users/nicol/OneDrive/nicodeGmbH/ESP32-Projects/dmx4esp/dmx4espTest/build/bootloader-prefix/tmp"
  "C:/Users/nicol/OneDrive/nicodeGmbH/ESP32-Projects/dmx4esp/dmx4espTest/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/nicol/OneDrive/nicodeGmbH/ESP32-Projects/dmx4esp/dmx4espTest/build/bootloader-prefix/src"
  "C:/Users/nicol/OneDrive/nicodeGmbH/ESP32-Projects/dmx4esp/dmx4espTest/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/nicol/OneDrive/nicodeGmbH/ESP32-Projects/dmx4esp/dmx4espTest/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/nicol/OneDrive/nicodeGmbH/ESP32-Projects/dmx4esp/dmx4espTest/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
