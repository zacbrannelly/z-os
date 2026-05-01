#!/bin/bash 

# Create EFI boot image
dd if=/dev/zero of=efiboot.img bs=1M count=16
mkfs.vfat efiboot.img
mmd -i efiboot.img ::/EFI
mmd -i efiboot.img ::/EFI/BOOT
mcopy -i efiboot.img Build/bootloader/DEBUG_GCC5/AARCH64/bootloader.efi ::/EFI/BOOT/BOOTAA64.EFI

# Create ISO image
mkdir -p iso
cp efiboot.img iso/

xorriso -as mkisofs \
  -R -J \
  -V UEFI_HELLO \
  -e efiboot.img \
  -no-emul-boot \
  -o uefi-hello.iso \
  iso/

# Move ISO image to Build directory (so it is accessible from the host)
mv uefi-hello.iso Build/bootloader
