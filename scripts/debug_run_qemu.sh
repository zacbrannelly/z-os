#!/bin/bash

ESP_DIR="$(pwd)/esp"

qemu-system-aarch64 \
  -machine virt \
  -cpu cortex-a72 \
  -m 1024 \
  -bios ./build/QEMU_EFI.fd \
  -drive if=virtio,format=raw,file=fat:rw:${ESP_DIR} \
  -serial stdio \
  -monitor none \
  -device ramfb \
  -device virtio-gpu-pci \
  -device qemu-xhci \
  -usb \
  -device usb-kbd \
  -S \
  -s
