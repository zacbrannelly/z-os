cmake_minimum_required(VERSION 3.25)

file(GLOB_RECURSE C_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

add_library(libgfx STATIC
    ${C_SOURCES}
)

target_include_directories(libgfx PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/3rdparty/openlibm/include"
    "${CMAKE_SOURCE_DIR}/3rdparty/openlibm/src"
)

target_compile_options(libgfx PRIVATE
    -ffreestanding
    -fno-builtin
    -fno-stack-protector
    -fno-pie
    -fno-pic
)

target_link_options(libgfx PRIVATE
    -nostdlib
    -no-pie
)

target_link_libraries(libgfx INTERFACE
    libz
    openlibm
    "${LIBGCC_FILE}"
)
