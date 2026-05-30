cmake_minimum_required(VERSION 3.25)

set(CMAKE_C_COMPILER gcc CACHE STRING "C compiler" FORCE)
set(CMAKE_ASM_COMPILER gcc CACHE STRING "ASM compiler" FORCE)

project(z_os LANGUAGES C ASM)

# Build options
option(ZOS_RUN_TESTS "Run tests only on boot" OFF)

# General CMake configuration for the in-container build.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Make sure the linker is aware of the page size we support (4096).
add_link_options(
    -Wl,-z,max-page-size=0x1000
    -Wl,-z,common-page-size=0x1000
)

if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "This build is currently configured to require GCC, but CMake selected ${CMAKE_C_COMPILER_ID}.")
endif()

execute_process(
    COMMAND "${CMAKE_C_COMPILER}" -print-libgcc-file-name
    OUTPUT_VARIABLE LIBGCC_FILE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT EXISTS "${LIBGCC_FILE}")
    message(FATAL_ERROR "Failed to locate libgcc using ${CMAKE_C_COMPILER}.")
endif()

# Final output paths.
set(ESP_DIR "${CMAKE_SOURCE_DIR}/esp" CACHE PATH "ESP staging directory")

# Include the 3rdparty targets (openlibm).
add_subdirectory(./3rdparty)

# Include the libz library (our version of libc).
add_subdirectory(./libz)

# Include userland libraries (e.g. libgfx)
add_subdirectory(./libraries)

# Include the kernel target.
add_subdirectory(./kernel)

# Include the bootloader target.
add_subdirectory(./loader)

# Include programs to boot on start.
add_subdirectory(./programs/compositor)
add_subdirectory(./programs/hello_world)

# Include test programs.
add_subdirectory(./tests)

get_property(KERNEL_ELF TARGET kernel PROPERTY OUTPUT_ELF_PATH)
get_property(BOOTLOADER_EFI TARGET bootloader PROPERTY OUTPUT_EFI_PATH)

# Stage the final boot artifacts into the ESP directory consumed by QEMU:
# the UEFI bootloader goes in EFI/BOOT, and the kernel ELF is copied beside it.
add_custom_target(esp
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ESP_DIR}/EFI/BOOT"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${BOOTLOADER_EFI}" "${ESP_DIR}/EFI/BOOT/BOOTAA64.EFI"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${KERNEL_ELF}" "${ESP_DIR}/kernel.elf"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:hello_world>" "${ESP_DIR}/hello_world.elf"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:compositor>" "${ESP_DIR}/compositor.elf"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:tests>" "${ESP_DIR}/tests.elf"
    DEPENDS bootloader kernel hello_world compositor
    COMMENT "Staging boot artifacts into the ESP directory"
    VERBATIM
)
