#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

"${script_dir}/cmake_build_entrypoint.sh" /opt/edk2/bootloader/build/cmake/release Release DEBUG
