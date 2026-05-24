cmake_minimum_required(VERSION 3.25)

# Gather all kernel compilation units. The kernel is built entirely from
# freestanding C/ASM sources under the kernel/ tree.
file(GLOB_RECURSE KERNEL_C_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
file(GLOB_RECURSE KERNEL_ASM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.S")

# Compile the kernel into objects only. We do not use CMake's normal executable
# link step because the original build linked the kernel directly with ld and a
# custom linker script, and we want to preserve that ELF layout.
add_library(kernel_objects OBJECT
    ${KERNEL_C_SOURCES}
    ${KERNEL_ASM_SOURCES}
)

# Kernel sources need direct access to the repo headers plus openlibm headers.
target_include_directories(kernel_objects PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/3rdparty/openlibm/include"
    "${CMAKE_SOURCE_DIR}/3rdparty/openlibm/src"
)

# Match the historical freestanding kernel compile flags as closely as possible.
target_compile_options(kernel_objects PRIVATE
    -ffreestanding
    -fno-builtin
    -fno-stack-protector
    -fno-strict-aliasing
    -fno-strict-overflow
    -fno-pie
    -fno-pic
)

if(ZOS_RUN_TESTS)
    target_compile_definitions(kernel_objects PUBLIC RUN_TESTS=1)
endif()

set(KERNEL_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/kernel/linker.ld")
set(KERNEL_ELF "${CMAKE_BINARY_DIR}/artifacts/kernel.elf")

# Recreate the old kernel final link exactly: invoke ld directly with the kernel
# linker script, all compiled kernel objects, and the static openlibm archive.
add_custom_target(kernel ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/artifacts"
    COMMAND /usr/bin/ld
        -T "${KERNEL_LINKER_SCRIPT}"
        -o "${KERNEL_ELF}"
        $<TARGET_OBJECTS:kernel_objects>
        "$<TARGET_FILE:openlibm>"
        "${LIBGCC_FILE}"
    DEPENDS
        kernel_objects
        openlibm
        "${KERNEL_LINKER_SCRIPT}"
    COMMENT "Linking kernel.elf with ld"
    COMMAND_EXPAND_LISTS
    VERBATIM
)

set_property(TARGET kernel PROPERTY OUTPUT_ELF_PATH "${KERNEL_ELF}")
