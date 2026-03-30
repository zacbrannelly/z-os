#!/bin/bash

set -euo pipefail

optimization_flag="${1:?missing optimization flag}"
object_root="${2:?missing object output directory}"
debug_flag="${3:-}"

mkdir -p "${object_root}"

mapfile -t kernel_sources < <(find MyLoader/kernel -name '*.c' | sort)

if [ "${#kernel_sources[@]}" -eq 0 ]; then
  echo "No kernel sources found" >&2
  exit 1
fi

object_files=()

for source_path in "${kernel_sources[@]}"; do
  relative_path="${source_path#MyLoader/kernel/}"
  object_path="${object_root}/${relative_path%.c}.o"

  mkdir -p "$(dirname "${object_path}")"

  gcc \
    -c "${source_path}" \
    -o "${object_path}" \
    -ffreestanding \
    -fno-builtin \
    -fno-stack-protector \
    "${optimization_flag}" \
    ${debug_flag:+"${debug_flag}"}

  object_files+=("${object_path}")
done

ld \
  -T MyLoader/kernel/linker.ld \
  -o Build/MyLoader/kernel.elf \
  "${object_files[@]}"
