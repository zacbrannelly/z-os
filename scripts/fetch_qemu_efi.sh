#!/bin/bash

set -euo pipefail

build_dir="$(pwd)/build"
target="${build_dir}/QEMU_EFI.fd"

if [ -f "${target}" ]; then
    echo "QEMU EFI firmware file already exists."
    exit 0
fi

mkdir -p "${build_dir}"

host_fd="/usr/share/qemu-efi-aarch64/QEMU_EFI.fd"
if [ -f "${host_fd}" ]; then
    echo "Using host-installed QEMU EFI firmware at ${host_fd}."
    cp "${host_fd}" "${target}"
    exit 0
fi

echo "Host firmware not found; fetching via Docker."

COMMANDS="
    apt-get update && \
    apt-get install -y qemu-efi-aarch64 && \
    cp /usr/share/qemu-efi-aarch64/QEMU_EFI.fd /build/QEMU_EFI.fd
"

docker run --rm \
    -v "${build_dir}:/build" \
    ubuntu:24.04 \
    bash -c "${COMMANDS}"
