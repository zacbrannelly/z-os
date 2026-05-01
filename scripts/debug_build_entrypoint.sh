#!/bin/bash

# Build the OS loader.
./bootloader/scripts/build_loader.sh

# Build the kernel.
./bootloader/scripts/debug_build_kernel.sh

# Make the ESP directory.
mkdir -p /esp/EFI/BOOT

# Copy the loader to the ESP directory.
cp ./Build/bootloader/DEBUG_GCC5/AARCH64/bootloader.efi /esp/EFI/BOOT/BOOTAA64.EFI

# Copy the kernel to the ESP directory.
cp ./Build/bootloader/kernel.elf /esp/kernel.elf
