#!/bin/bash

# Build the OS loader.
./MyLoader/scripts/build_loader.sh

# Build the kernel.
./MyLoader/scripts/debug_build_kernel.sh

# Make the ESP directory.
mkdir -p /esp/EFI/BOOT

# Copy the loader to the ESP directory.
cp ./Build/MyLoader/DEBUG_GCC5/AARCH64/MyLoader.efi /esp/EFI/BOOT/BOOTAA64.EFI

# Copy the kernel to the ESP directory.
cp ./Build/MyLoader/kernel.elf /esp/kernel.elf
