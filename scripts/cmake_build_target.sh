#!/bin/bash

set -euo pipefail

build_dir="${1:?missing build directory}"
cmake_build_type="${2:?missing CMake build type}"
edk2_build_target="${3:?missing EDK2 build target}"
target_name="${4:?missing target name}"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

cmake \
  -S "${repo_root}" \
  -B "${build_dir}" \
  -DCMAKE_BUILD_TYPE="${cmake_build_type}" \
  -DEDK2_BUILD_TARGET="${edk2_build_target}"

cmake --build "${build_dir}" --target "${target_name}"
