cmake_minimum_required(VERSION 3.25)

file(GLOB_RECURSE C_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

add_library(libcommon STATIC
    ${C_SOURCES}
)

target_include_directories(libcommon PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}"
)

target_compile_options(libcommon PRIVATE
    -ffreestanding
    -fno-builtin
    -fno-stack-protector
    -fno-pie
    -fno-pic
)

target_link_options(libcommon PRIVATE
    -nostdlib
    -no-pie
)

target_link_libraries(libcommon INTERFACE
    libz
    "${LIBGCC_FILE}"
)
