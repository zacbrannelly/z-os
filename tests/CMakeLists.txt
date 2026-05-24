cmake_minimum_required(VERSION 3.25)

set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld")

file(GLOB_RECURSE C_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
file(GLOB_RECURSE ASM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.S")

add_executable(tests
    ${C_SOURCES}
    ${ASM_SOURCES}
    ${CMAKE_SOURCE_DIR}/libz/crt0.S
)

target_include_directories(tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}"
)

target_compile_options(tests PRIVATE
    -ffreestanding
    -fno-builtin
    -fno-stack-protector
    -fno-pie
    -fno-pic
)

target_link_options(tests PRIVATE
    -nostdlib
    -no-pie
    -Wl,-T,${LINKER_SCRIPT}
    -Wl,-Map,${CMAKE_BINARY_DIR}/artifacts/tests.map
)

target_link_libraries(tests PRIVATE
    libz
    "${LIBGCC_FILE}"
)

set_target_properties(tests PROPERTIES
    OUTPUT_NAME "tests"
    SUFFIX ".elf"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/artifacts"
)
