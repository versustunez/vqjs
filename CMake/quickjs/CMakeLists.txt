cmake_minimum_required(VERSION 3.9)
project(quickjs LANGUAGES C)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD 11)

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, default to Release")
    set(CMAKE_BUILD_TYPE "Release")
endif ()

message(STATUS "Building in ${CMAKE_BUILD_TYPE} mode")
message(STATUS "Building with ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION} on ${CMAKE_SYSTEM}")

if (MSVC)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
endif ()

if (CMAKE_BUILD_TYPE MATCHES "Debug")
    add_compile_options(-O0 -g)
endif ()

add_definitions(-DJS_NAN_BOXING=0)

set(qjs_raw_sources
        cutils.c
        libbf.c
        libregexp.c
        libunicode.c
        quickjs.c
        quickjs-libc.c
)

set(qjs_sources "")
foreach(source ${qjs_raw_sources})
    list(APPEND qjs_sources "${CMAKE_SOURCE_DIR}/vendor/quickjs/${source}")
endforeach()

list(APPEND qjs_defines _GNU_SOURCE)
list(APPEND qjs_libs qjs ${CMAKE_DL_LIBS})
if (NOT MSVC)
    list(APPEND qjs_libs m pthread)
endif ()

add_library(qjs ${qjs_sources})
target_compile_definitions(qjs PRIVATE ${qjs_defines})
if (CMAKE_BUILD_TYPE MATCHES Debug OR DUMP_LEAKS)
    target_compile_definitions(qjs PRIVATE
            DUMP_LEAKS
    )
endif ()

target_include_directories(qjs PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)