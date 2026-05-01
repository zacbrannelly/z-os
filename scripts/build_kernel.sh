#!/bin/bash

set -euo pipefail

./bootloader/scripts/build_kernel_common.sh -O2 Build/bootloader/kernel_objects
