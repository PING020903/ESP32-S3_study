# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/ESP_MCU_tool/ESP_IDF_TOOLS/Espressif/frameworks/esp-idf-v5.1.2/components/bootloader/subproject"
  "C:/Users/12566/Desktop/get-started/espS3_project/ESP_S3_project/build/bootloader"
  "C:/Users/12566/Desktop/get-started/espS3_project/ESP_S3_project/build/bootloader-prefix"
  "C:/Users/12566/Desktop/get-started/espS3_project/ESP_S3_project/build/bootloader-prefix/tmp"
  "C:/Users/12566/Desktop/get-started/espS3_project/ESP_S3_project/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/12566/Desktop/get-started/espS3_project/ESP_S3_project/build/bootloader-prefix/src"
  "C:/Users/12566/Desktop/get-started/espS3_project/ESP_S3_project/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/12566/Desktop/get-started/espS3_project/ESP_S3_project/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/12566/Desktop/get-started/espS3_project/ESP_S3_project/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
