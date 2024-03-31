# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB app_sources 
${CMAKE_SOURCE_DIR}/src/esp32/hardware_esp32.*
${CMAKE_SOURCE_DIR}/src/sd_loader/*.*
${CMAKE_SOURCE_DIR}/src/hardware.h
${CMAKE_SOURCE_DIR}/src/helpers.*
${CMAKE_SOURCE_DIR}/src/FAT32.*
${CMAKE_SOURCE_DIR}/src/SD_routines.*
${CMAKE_SOURCE_DIR}/src/SPI_routines.*
${CMAKE_SOURCE_DIR}/src/Serial.*)
idf_component_register(SRCS ${app_sources})
