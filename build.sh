#!/bin/bash

set -euo pipefail

ZOS_RUN_TESTS="${ZOS_RUN_TESTS:-OFF}"

if [ "${ZOS_RUN_TESTS}" = "ON" ]; then
    cmake_subdir="test"
    edk2_subdir="edk2-test"
else
    cmake_subdir="release"
    edk2_subdir="edk2"
fi

mkdir -p "$(pwd)/build/${edk2_subdir}" "$(pwd)/build/cmake/${cmake_subdir}" "$(pwd)/esp"

docker run --rm -it \
    -v $(pwd):/opt/edk2/bootloader \
    -v $(pwd)/build/${edk2_subdir}:/opt/edk2/Build/bootloader \
    -v $(pwd)/esp:/esp \
    -e ZOS_RUN_TESTS="${ZOS_RUN_TESTS}" \
    -w /opt/edk2 \
    z-os-dev:latest \
    bash -c "./bootloader/scripts/build_entrypoint.sh"
