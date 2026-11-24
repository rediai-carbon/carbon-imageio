cmake_minimum_required(VERSION 3.16)
project(imageio_jpeg_png_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(carbon-imageio CONFIG REQUIRED)

add_executable(imageio_jpeg_png_consumer main.cpp)
target_link_libraries(imageio_jpeg_png_consumer PRIVATE CcpImageIO)
