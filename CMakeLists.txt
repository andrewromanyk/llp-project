cmake_minimum_required(VERSION 3.20)
project(audio_tool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Raylib provided by Conan (CMakeDeps generator)
find_package(raylib CONFIG REQUIRED)

# On Linux, find native ALSA from the system
if(NOT WIN32)
    find_package(ALSA REQUIRED)
endif()

set(SOURCES
    src/main.cpp
    src/dsp.cpp
    src/ui.cpp
)

# compile only the audio driver for the host OS
if(WIN32)
    message(STATUS "Configuring for Windows (WASAPI backend)")
    list(APPEND SOURCES src/audio_win32.cpp)
else()
    message(STATUS "Configuring for Linux (ALSA backend)")
    list(APPEND SOURCES src/audio_linux.cpp)
endif()

add_executable(audio_tool ${SOURCES})

target_include_directories(audio_tool PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(audio_tool PRIVATE raylib)

if(WIN32)
    # Windows native COM and Media Foundation APIs for WASAPI
    target_link_libraries(audio_tool PRIVATE ole32 mfplat)
else()
    # Linux ALSA and POSIX threading/math
    target_link_libraries(audio_tool PRIVATE 
        ALSA::ALSA 
        pthread 
        m 
        dl
    )
endif()
