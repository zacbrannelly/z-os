cmake_minimum_required(VERSION 3.25)

file(GLOB_RECURSE LOADER_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*")

set(BOOTLOADER_STAMP "${CMAKE_BINARY_DIR}/artifacts/bootloader.stamp")

# Build the UEFI bootloader through EDK2. CMake orchestrates the step, but the
# actual bootloader build still happens through EDK2's build system.
add_custom_command(
    OUTPUT "${BOOTLOADER_STAMP}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/artifacts"
    COMMAND /bin/bash -c "set -eo pipefail; cd /opt/edk2; source edksetup.sh; build -a AARCH64 -t GCC5 -p ${CMAKE_CURRENT_SOURCE_DIR}/bootloader.dsc -b ${EDK2_BUILD_TARGET_UPPER}"
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
