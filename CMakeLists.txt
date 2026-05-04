cmake_minimum_required(VERSION 3.25)

project(z_os LANGUAGES C ASM)

# General CMake configuration for the in-container build.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Kernel related paths.
set(KERNEL_ELF "${CMAKE_BINARY_DIR}/artifacts/kernel.elf")

# Bootloader related paths.
set(EDK2_BUILD_TARGET "DEBUG" CACHE STRING "EDK2 build target to pass to the EDK2 build command")
string(TOUPPER "${EDK2_BUILD_TARGET}" EDK2_BUILD_TARGET_UPPER)
set(EDK2_OUTPUT_DIR "/opt/edk2/Build/bootloader" CACHE PATH "EDK2 output directory inside the build container")
set(BOOTLOADER_EFI "${EDK2_OUTPUT_DIR}/${EDK2_BUILD_TARGET_UPPER}_GCC5/AARCH64/bootloader.efi")

# Final output paths.
set(ESP_DIR "${CMAKE_SOURCE_DIR}/esp" CACHE PATH "ESP staging directory")

# Include the 3rdparty targets (openlibm).
add_subdirectory(./3rdparty)

# Include the kernel target.
add_subdirectory(./kernel)

# Include the bootloader target.
add_subdirectory(./loader)

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
