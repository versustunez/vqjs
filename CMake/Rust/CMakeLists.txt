set(RUST_DIR ${CMAKE_SOURCE_DIR}/ts-support)
set(RUST_TARGET_DIR ${RUST_DIR}/target)

find_program(CARGO cargo REQUIRED)

if (MSVC)
    set(RUST_LIB_NAME ts_support.lib)
else ()
    set(RUST_LIB_NAME libts_support.a)
endif ()

set(RUST_DEBUG_DIR ${RUST_TARGET_DIR}/debug)
set(RUST_RELEASE_DIR ${RUST_TARGET_DIR}/release)

set(RUST_DEBUG_LIB ${RUST_DEBUG_DIR}/${RUST_LIB_NAME})
set(RUST_RELEASE_LIB ${RUST_RELEASE_DIR}/${RUST_LIB_NAME})

add_custom_target(rustlib ALL
        COMMAND ${CARGO} build
        $<$<CONFIG:Release>:--release>
        WORKING_DIRECTORY ${RUST_DIR}
        BYPRODUCTS
        $<$<CONFIG:Release>:${RUST_RELEASE_LIB}>
        $<$<CONFIG:Debug>:${RUST_DEBUG_LIB}>
)

add_library(ts_support STATIC IMPORTED GLOBAL)

set_target_properties(ts_support PROPERTIES
        IMPORTED_LOCATION_DEBUG "${RUST_DEBUG_LIB}"
        IMPORTED_LOCATION_RELEASE "${RUST_RELEASE_LIB}"
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
        MAP_IMPORTED_CONFIG_MINSIZEREL Release
)

add_dependencies(ts_support rustlib)
