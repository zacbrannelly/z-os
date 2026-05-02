#!/bin/bash

set -euo pipefail

build_dir="${1:?missing build directory}"
cmake_build_type="${2:?missing CMake build type}"
edk2_build_target="${3:?missing EDK2 build target}"

"$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/cmake_build_target.sh" \
  "${build_dir}" \
  "${cmake_build_type}" \
  "${edk2_build_target}" \
  esp
