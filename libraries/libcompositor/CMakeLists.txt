cmake_minimum_required(VERSION 3.25)

file(GLOB_RECURSE C_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

add_library(libcompositor STATIC
    ${C_SOURCES}
)

target_include_directories(libcompositor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}"
)

target_compile_options(libcompositor PRIVATE
    -ffreestanding
    -fno-builtin
    -fno-stack-protector
    -fno-pie
    -fno-pic
)

target_link_options(libcompositor PRIVATE
    -nostdlib
    -no-pie
)

target_link_libraries(libcompositor INTERFACE
    libz
    "${LIBGCC_FILE}"
)
