cmake_minimum_required(VERSION 3.29)
set(Name VQJS)
project(${Name})
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
set(IS_DEBUG_BUILD CMAKE_BUILD_TYPE STREQUAL "Debug")
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(RUST_DIR ${CMAKE_SOURCE_DIR}/ts-support)
set(RUST_TARGET_DIR ${RUST_DIR}/target/release)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(MOLD_PROGRAM mold)
    if (MOLD_PROGRAM)
        set(CMAKE_LINKER_TYPE MOLD)
    endif ()
endif ()

if (MSVC)
    SET(TS_LIB "ts_support.lib")
else ()
    SET(TS_LIB "libts_support.a")
endif ()

# ASAN CHECK!
#set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined,alignment,leak,null,return -fsanitize-address-use-after-scope")
#set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address,undefined,alignment,leak,null,return -fsanitize-address-use-after-scope")

find_program(CARGO cargo REQUIRED)

add_library(sse4_1 INTERFACE IMPORTED)
target_compile_options(sse4_1 INTERFACE -msse4.1)

# PREPARE CMAKE
include(Version)

if (${IS_DEBUG_BUILD})
    add_compile_definitions(VQJSDEBUG)
endif ()

add_definitions(-DJS_NAN_BOXING=0)
add_definitions(-DMI_MALLOC_OVERRIDE=1)

# ADD Directories
add_subdirectory(CMake)
add_subdirectory(src)
add_subdirectory(CMake/Rust)


get_property(source_files GLOBAL PROPERTY SRCS)
add_library(${PROJECT_NAME} SHARED ${source_files} vendor/mimalloc/src/static.c src/Context.cpp)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_target_properties(${PROJECT_NAME} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/
        OUTPUT_NAME "VQJS"
        PREFIX ""
        DEBUG_POSTFIX ""
        RELEASE_POSTFIX ""
        WINDOWS_EXPORT_ALL_SYMBOLS ON
)
add_dependencies(${Name} rustlib)

target_include_directories(${Name} PUBLIC ${CMAKE_BINARY_DIR})
target_include_directories(${Name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/includes/)
target_include_directories(${Name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/)
target_include_directories(${Name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/mimalloc/include)

# ADD quickjs-ng to CMAKE...
add_subdirectory(CMake/quickjs)

# We link vqjs against qjs because vqjs needs it
if (MSVC)
    target_link_libraries(${Name} PRIVATE qjs ts_support ws2_32
            userenv
            ntdll)
else ()
    target_link_libraries(${Name} PRIVATE qjs ts_support)
endif ()


if (!MSVC)
    add_custom_target(
            copy-compile-commands ALL
            ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/compile_commands.json
            ${CMAKE_CURRENT_LIST_DIR}
    )
endif ()
