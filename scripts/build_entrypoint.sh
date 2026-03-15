#!/bin/bash

# Build the OS loader.
./MyLoader/scripts/build_loader.sh

# Make the ESP directory.
mkdir -p /esp/EFI/BOOT

# Copy the loader to the ESP directory.
cp ./Build/MyLoader/DEBUG_GCC5/AARCH64/MyLoader.efi /esp/EFI/BOOT/BOOTAA64.EFI
