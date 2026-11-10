#!/bin/bash

ESP_DIR="$(pwd)/esp"

display_args=()
if [ "${ZOS_RUN_TESTS:-OFF}" = "ON" ]; then
  display_args+=(-display none)
elif [ "$(uname -s)" = "Darwin" ]; then
  display_args+=(-display cocoa,zoom-to-fit=on)
fi

qemu-system-aarch64 \
  "${display_args[@]}" \
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
  -device usb-mouse \
  -S \
  -s
