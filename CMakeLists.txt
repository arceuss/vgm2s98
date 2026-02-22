cmake_minimum_required(VERSION 3.10)
project(vgm2s98)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    vgm2s98.cpp
    vgm_reader.cpp
    s98_writer.cpp
)

add_executable(vgm2s98 ${SOURCES})

target_include_directories(vgm2s98 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(vgm2s98 PRIVATE /W4)
else()
    target_compile_options(vgm2s98 PRIVATE -Wall -Wextra)
    target_link_libraries(vgm2s98 m)
endif()
