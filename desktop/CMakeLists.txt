cmake_minimum_required(VERSION 3.25)

project(pocketwalker)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGLWidgets Multimedia)
find_package(OpenGL REQUIRED)

add_subdirectory(${CMAKE_SOURCE_DIR}/../core core)

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/../external/json nlohmann)
add_subdirectory(${CMAKE_SOURCE_DIR}/../external/argparse argparse)

file(GLOB_RECURSE SOURCES src/*.cpp src/*.h)

if(WIN32)
    set(ICON_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
endif()

if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set(EXE_TYPE WIN32)
endif()

add_executable(pocketwalker ${EXE_TYPE} ${SOURCES} ${ICON_RESOURCE})

set_target_properties(pocketwalker PROPERTIES
        AUTOMOC ON
        AUTORCC ON
        OUTPUT_NAME "pocketwalker"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/release"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/release"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/release"
)

target_sources(pocketwalker PRIVATE assets/resources.qrc)

target_include_directories(pocketwalker PRIVATE
        ${CMAKE_SOURCE_DIR}/../external/argparse/include
)

target_link_libraries(pocketwalker PRIVATE
        pocketwalker_core
        Qt6::Core
        Qt6::Widgets
        Qt6::OpenGLWidgets
        Qt6::Multimedia
        OpenGL::GL
        nlohmann_json::nlohmann_json
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(pocketwalker PRIVATE stdc++exp)
    if(WIN32)
        target_link_options(pocketwalker PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
endif()
