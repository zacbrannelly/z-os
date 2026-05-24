#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ "${ZOS_RUN_TESTS:-OFF}" = "ON" ]; then
    build_subdir="test"
else
    build_subdir="release"
fi

"${script_dir}/cmake_build_entrypoint.sh" "/opt/edk2/bootloader/build/cmake/${build_subdir}" Release DEBUG
