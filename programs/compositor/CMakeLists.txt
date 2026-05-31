cmake_minimum_required(VERSION 3.25)

set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld")

file(GLOB_RECURSE C_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
file(GLOB_RECURSE ASM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.S")

add_executable(compositor
    ${C_SOURCES}
    ${ASM_SOURCES}
    ${CMAKE_SOURCE_DIR}/libz/crt0.S
)

target_include_directories(compositor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/libraries"
)

target_compile_options(compositor PRIVATE
    -ffreestanding
    -fno-builtin
    -fno-stack-protector
    -fno-pie
    -fno-pic
)

target_link_options(compositor PRIVATE
    -nostdlib
    -no-pie
    -Wl,-T,${LINKER_SCRIPT}
    -Wl,-Map,${CMAKE_BINARY_DIR}/artifacts/compositor.map
)

target_link_libraries(compositor PRIVATE
    libz
    libgfx
    libinput
    "${LIBGCC_FILE}"
)

set_target_properties(compositor PROPERTIES
    OUTPUT_NAME "compositor"
    SUFFIX ".elf"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/artifacts"
)
