#!/bin/bash

ESP_DIR="$(pwd)/esp"

qemu_args=(
  -machine virt
  -cpu cortex-a72
  -m 1024
  -bios ./build/QEMU_EFI.fd
  -drive if=virtio,format=raw,file=fat:rw:${ESP_DIR}
  -serial stdio
  -monitor none
)

qemu_args+=(
  -device ramfb
  -device virtio-gpu-pci
  -device qemu-xhci
  -usb
  -device usb-kbd
  -device usb-mouse
)

if [ "${ZOS_RUN_TESTS:-OFF}" = "ON" ]; then
  qemu_args+=(-display none)
fi

qemu-system-aarch64 "${qemu_args[@]}"
