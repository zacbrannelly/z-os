#!/bin/bash

set -euo pipefail

build_target="${1:?missing EDK2 build target}"

cd /opt/edk2

bash -c '
  set -euo pipefail
  source ./edksetup.sh
  build -a AARCH64 -t GCC5 -p bootloader/loader/bootloader.dsc -b "$1"
' bash "${build_target}"
