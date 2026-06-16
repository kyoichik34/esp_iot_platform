cmake_minimum_required(VERSION 3.22)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_presence)
spiffs_create_partition_image(spiffs ${PROJECT_DIR}/spiffs FLASH_IN_PROJECT)
