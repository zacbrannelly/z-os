cmake_minimum_required(VERSION 3.25)

project(z_os LANGUAGES C ASM)

# General CMake configuration for the in-container build.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Paths and knobs used by the Docker/EDK2 build flow.
set(EDK2_BUILD_TARGET "DEBUG" CACHE STRING "EDK2 build target to pass to the EDK2 build command")
set(EDK2_OUTPUT_DIR "/opt/edk2/Build/bootloader" CACHE PATH "EDK2 output directory inside the build container")
set(ESP_DIR "${CMAKE_SOURCE_DIR}/esp" CACHE PATH "ESP staging directory")

# Build openlibm as a static dependency for the freestanding kernel.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build dependencies as static libraries" FORCE)
add_subdirectory(3rdparty/openlibm EXCLUDE_FROM_ALL)

# Gather all kernel compilation units. The kernel is built entirely from
# freestanding C/ASM sources under the kernel/ tree.
file(GLOB_RECURSE KERNEL_C_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/kernel/*.c")
file(GLOB_RECURSE KERNEL_ASM_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/kernel/*.S")
file(GLOB_RECURSE LOADER_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/loader/*")

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

set(KERNEL_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/kernel/linker.ld")
set(KERNEL_ELF "${CMAKE_BINARY_DIR}/artifacts/kernel.elf")

# openlibm's own CMake target exports host-style interface flags (for example
# strict C standard settings). Clear those exported interface properties so
# linking openlibm does not change how the kernel itself is compiled.
target_include_directories(openlibm SYSTEM PRIVATE
    "${CMAKE_SOURCE_DIR}/3rdparty/openlibm/include"
    "${CMAKE_SOURCE_DIR}/3rdparty/openlibm/src"
)

get_target_property(OPENLIBM_COMPILE_OPTIONS openlibm INTERFACE_COMPILE_OPTIONS)
if(OPENLIBM_COMPILE_OPTIONS)
    set_target_properties(openlibm PROPERTIES INTERFACE_COMPILE_OPTIONS "")
endif()

get_target_property(OPENLIBM_INCLUDE_DIRECTORIES openlibm INTERFACE_INCLUDE_DIRECTORIES)
if(OPENLIBM_INCLUDE_DIRECTORIES)
    set_target_properties(openlibm PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "")
endif()

# Recreate the old kernel final link exactly: invoke ld directly with the kernel
# linker script, all compiled kernel objects, and the static openlibm archive.
add_custom_target(kernel ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/artifacts"
    COMMAND /usr/bin/ld
        -T "${KERNEL_LINKER_SCRIPT}"
        -o "${KERNEL_ELF}"
        $<TARGET_OBJECTS:kernel_objects>
        "$<TARGET_FILE:openlibm>"
    DEPENDS
        kernel_objects
        openlibm
        "${KERNEL_LINKER_SCRIPT}"
    COMMENT "Linking kernel.elf with ld"
    COMMAND_EXPAND_LISTS
    VERBATIM
)

string(TOUPPER "${EDK2_BUILD_TARGET}" EDK2_BUILD_TARGET_UPPER)
set(BOOTLOADER_EFI "${EDK2_OUTPUT_DIR}/${EDK2_BUILD_TARGET_UPPER}_GCC5/AARCH64/bootloader.efi")
set(BOOTLOADER_STAMP "${CMAKE_BINARY_DIR}/artifacts/bootloader.stamp")

# Build the UEFI bootloader through EDK2. CMake orchestrates the step, but the
# actual bootloader build still happens through EDK2's build system.
add_custom_command(
    OUTPUT "${BOOTLOADER_STAMP}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/artifacts"
    COMMAND /bin/bash -c "set -eo pipefail; cd /opt/edk2; source edksetup.sh; build -a AARCH64 -t GCC5 -p bootloader/loader/bootloader.dsc -b ${EDK2_BUILD_TARGET_UPPER}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${BOOTLOADER_STAMP}"
    BYPRODUCTS "${BOOTLOADER_EFI}"
    DEPENDS
        ${LOADER_SOURCES}
    WORKING_DIRECTORY /opt/edk2
    COMMENT "Building EDK2 bootloader (${EDK2_BUILD_TARGET_UPPER})"
    VERBATIM
)

add_custom_target(bootloader
    DEPENDS "${BOOTLOADER_STAMP}"
)

# Stage the final boot artifacts into the ESP directory consumed by QEMU:
# the UEFI bootloader goes in EFI/BOOT, and the kernel ELF is copied beside it.
add_custom_target(esp
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ESP_DIR}/EFI/BOOT"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${BOOTLOADER_EFI}" "${ESP_DIR}/EFI/BOOT/BOOTAA64.EFI"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${KERNEL_ELF}" "${ESP_DIR}/kernel.elf"
    DEPENDS bootloader kernel
    COMMENT "Staging boot artifacts into the ESP directory"
    VERBATIM
)
