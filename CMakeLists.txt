cmake_minimum_required(VERSION 3.25)

project(z_os LANGUAGES C ASM)

# General CMake configuration for the in-container build.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Final output paths.
set(ESP_DIR "${CMAKE_SOURCE_DIR}/esp" CACHE PATH "ESP staging directory")

# Include the 3rdparty targets (openlibm).
add_subdirectory(./3rdparty)

# Include the kernel target.
add_subdirectory(./kernel)

# Include the bootloader target.
add_subdirectory(./loader)

# Include hello world program.
add_subdirectory(./programs/hello_world)

get_property(KERNEL_ELF TARGET kernel PROPERTY OUTPUT_ELF_PATH)
get_property(BOOTLOADER_EFI TARGET bootloader PROPERTY OUTPUT_EFI_PATH)

# Stage the final boot artifacts into the ESP directory consumed by QEMU:
# the UEFI bootloader goes in EFI/BOOT, and the kernel ELF is copied beside it.
add_custom_target(esp
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ESP_DIR}/EFI/BOOT"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${BOOTLOADER_EFI}" "${ESP_DIR}/EFI/BOOT/BOOTAA64.EFI"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${KERNEL_ELF}" "${ESP_DIR}/kernel.elf"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:hello_world>" "${ESP_DIR}/hello_world.elf"
    DEPENDS bootloader kernel hello_world
    COMMENT "Staging boot artifacts into the ESP directory"
    VERBATIM
)
