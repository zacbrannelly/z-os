#!/bin/bash

set -euo pipefail

optimization_flag="${1:?missing optimization flag}"
object_root="${2:?missing object output directory}"
debug_flag="${3:-}"

# Build the openlibm library (if the static library does not exist).
if [ ! -f "${object_root}/openlibm_build/libopenlibm.a" ]; then
  mkdir -p "${object_root}/openlibm_build"
  pushd "${object_root}/openlibm_build"
  cmake -DBUILD_SHARED_LIBS=OFF "/opt/edk2/bootloader/3rdparty/openlibm"
  cmake --build .
  popd
fi

mkdir -p "${object_root}"

mapfile -t kernel_sources < <(find bootloader/kernel \( -name '*.c' -o -name '*.S' \) | sort)

if [ "${#kernel_sources[@]}" -eq 0 ]; then
  echo "No kernel sources found" >&2
  exit 1
fi

object_files=()

for source_path in "${kernel_sources[@]}"; do
  relative_path="${source_path#bootloader/kernel/}"
  object_path="${object_root}/${relative_path%.c}.o"

  mkdir -p "$(dirname "${object_path}")"

  gcc \
    -c "${source_path}" \
    -o "${object_path}" \
    -I bootloader/3rdparty/openlibm/include \
    -I bootloader/3rdparty/openlibm/src \
    -ffreestanding \
    -fno-builtin \
    -fno-stack-protector \
    -fno-strict-aliasing \
    -fno-strict-overflow \
    "${optimization_flag}" \
    ${debug_flag:+"${debug_flag}"}

  object_files+=("${object_path}")
done

ld \
  -T bootloader/kernel/linker.ld \
  -o Build/bootloader/kernel.elf \
  "${object_files[@]}" \
  "${object_root}/openlibm_build/libopenlibm.a"
