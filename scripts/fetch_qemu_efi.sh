#!/bin/bash

# Check if QEMU EFI firmware file already exists.
if [ -f $(pwd)/build/QEMU_EFI.fd ]; then
    echo "QEMU EFI firmware file already exists."
    exit 0
fi

COMMANDS="
    apt-get update && \
    apt-get install -y qemu-efi-aarch64 && \
    cp /usr/share/qemu-efi-aarch64/QEMU_EFI.fd /build/QEMU_EFI.fd
"

docker run --rm -it \
    -v $(pwd)/build:/build \
    ubuntu:24.04 \
    bash -c "${COMMANDS}"
