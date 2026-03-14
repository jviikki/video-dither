cmake_minimum_required(VERSION 3.10)

project(video-dither LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(OpenCV REQUIRED)

set(SOURCES
    src/main.cpp
    src/argparse.cpp
    src/concurrency.cpp
    src/video.cpp
    src/bv1d_encoder.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

include_directories(${OpenCV_INCLUDE_DIRS})

message(STATUS "OpenCV_LIBS: ${OpenCV_LIBS}")

target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})

add_executable(bv1d-player
    src/bv1d_player.cpp
    src/bv1d_decoder.cpp
)
target_link_libraries(bv1d-player ${OpenCV_LIBS})

# include_directories(${CMAKE_SOURCE_DIR}/include)

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
