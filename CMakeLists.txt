cmake_minimum_required(VERSION 3.24)
project(SnapPin LANGUAGES CXX)

# -------- Global settings --------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Option files
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Options)
include(Warnings)

# -------- Output dirs --------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# -------- Targets --------
add_subdirectory(src/core)
add_subdirectory(src/ui)
add_subdirectory(src/capture)
add_subdirectory(src/export)

if(SNAPPIN_ENABLE_OCR)
  add_subdirectory(src/ocr)
endif()

if(SNAPPIN_ENABLE_SCROLL)
  add_subdirectory(src/scroll)
endif()

if(SNAPPIN_ENABLE_RECORD)
  add_subdirectory(src/record)
endif()

add_subdirectory(src/app)

if(SNAPPIN_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
