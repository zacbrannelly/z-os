#!/bin/bash

docker run --rm -it \
    -v $(pwd):/opt/edk2/MyLoader \
    -v $(pwd)/build:/opt/edk2/Build/MyLoader \
    -v $(pwd)/esp:/esp \
    z-os-dev:latest \
    bash -c "./MyLoader/scripts/debug_build_entrypoint.sh"
