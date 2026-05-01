#!/bin/bash

set -euo pipefail

./bootloader/scripts/build_kernel_common.sh -O0 Build/bootloader/kernel_objects/debug -g
