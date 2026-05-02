#!/bin/bash

set -euo pipefail

mkdir -p "$(pwd)/build/edk2" "$(pwd)/build/cmake/debug" "$(pwd)/esp"

docker run --rm -it \
    -v $(pwd):/opt/edk2/bootloader \
    -v $(pwd)/build/edk2:/opt/edk2/Build/bootloader \
    -v $(pwd)/esp:/esp \
    -w /opt/edk2 \
    z-os-dev:latest \
    bash -c "./bootloader/scripts/debug_build_entrypoint.sh"
