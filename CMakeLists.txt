cmake_minimum_required(VERSION 3.22)
set(Name VQJS)
project(${Name})
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/)

if (MSVC)
    message(FATAL_ERROR "Unsupported compiler")
endif ()

add_library(sse4_1 INTERFACE IMPORTED)
target_compile_options(sse4_1 INTERFACE -msse4.1)

# MAC ARM and X86_64 Build
option(UniversalBinary "Build universal binary for mac" ON)
if (UniversalBinary)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE INTERNAL "")
endif ()

# PREPARE CMAKE
include(Version)

# ADD Directories
add_subdirectory(CMake)
add_subdirectory(src)

get_property(source_files GLOBAL PROPERTY SRCS)
add_library(${PROJECT_NAME} ${source_files})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_target_properties(${PROJECT_NAME} PROPERTIES
        ARCHIVE_OUTPUT_NAME_DEBUG "vqjs.debug"
        ARCHIVE_OUTPUT_NAME_RELEASE "vqjs.release"
        PREFIX ""
)

target_include_directories(${Name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/includes/)
target_include_directories(${Name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/)


add_custom_target(
        copy-compile-commands ALL
        ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_CURRENT_LIST_DIR}
)


# ADD quickjs-ng to CMAKE...
add_subdirectory(CMake/quickjs)

# We link vqjs against qjs because vqjs needs it
target_link_libraries(${Name} qjs)

# NOTE:
# We want to use this vqjs wrapper like this in V3D... the Wrapper allows to have an Header that is Abstracted and the implementation that uses qjs
# https://github.com/tomlankhorst/cmake-prebuilt-library
