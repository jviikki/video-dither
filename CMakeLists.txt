cmake_minimum_required(VERSION 3.10)

project(video-dither LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

set(SOURCES
    src/main.cpp
    src/argparse.cpp
    src/concurrency.cpp
    src/video.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

include_directories(${OpenCV_INCLUDE_DIRS})

message(STATUS "OpenCV_LIBS: ${OpenCV_LIBS}")

target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})

# include_directories(${CMAKE_SOURCE_DIR}/include)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
