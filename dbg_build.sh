#!/bin/bash

docker run --rm -it \
    -v $(pwd):/opt/edk2/bootloader \
    -v $(pwd)/build:/opt/edk2/Build/bootloader \
    -v $(pwd)/esp:/esp \
    z-os-dev:latest \
    bash -c "./bootloader/scripts/debug_build_entrypoint.sh"
