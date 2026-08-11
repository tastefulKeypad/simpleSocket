cmake_minimum_required(VERSION 3.28)
project(examples)

# Only UNIX/WIN32 platforms are supported
if (NOT UNIX AND NOT WIN32)
    message(FATAL_ERROR "Current platform is not supported")
endif()

# Initialize vars & packages
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
find_package(Curses REQUIRED)

# Create simpleFramework interface
add_library(SimpleFramework INTERFACE)
target_sources(SimpleFramework
    INTERFACE
        FILE_SET HEADERS
    )
target_include_directories(SimpleFramework
    INTERFACE 
        src
    )

# Collect all example files into a single list
set(exampleDirectories example)
set(exampleFiles)
foreach (exampleDirectory ${exampleDirectories})
    file(GLOB example ${exampleDirectory}/*.cpp)
    list(APPEND exampleFiles ${example})
endforeach ()


# Build every example 
message("### Building examples for target ${CMAKE_SYSTEM_NAME} platform")
foreach (example ${exampleFiles})
    set(FILE_SPECIFIC_FLAGS "")
    cmake_path(GET example FILENAME exampleName)
    cmake_path(GET example PARENT_PATH exampleParentDirectory)
    string(REPLACE ".cpp" "" exampleName ${exampleName})
    add_executable(${exampleName} ${example})
    target_link_libraries(${exampleName} PUBLIC SimpleFramework)
    set_target_properties(${exampleName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${exampleParentDirectory}")
    message("- Building ${exampleName}")

    # Check and link specific libraries
    file(READ ${example} exampleReadSource)
    if (exampleReadSource MATCHES "^[ ]*#include .*ncurses.h")
        message("#! ${exampleName} uses ncurses and requires POSIX compliant environment to run")
        target_link_libraries(${exampleName} PRIVATE ${CURSES_LIBRARIES})
        if (WIN32)
            list(APPEND FILE_SPECIFIC_FLAGS -DNCURSES_STATIC)
        endif()
    endif()

    target_compile_options(${exampleName} PRIVATE ${FILE_SPECIFIC_FLAGS})
endforeach()
