#!/bin/bash

set -e

export ZOS_RUN_TESTS=ON

./scripts/fetch_qemu_efi.sh
./build.sh

./scripts/run_qemu.sh | awk '
    { print; fflush() }
    /All tests passed/ { found=1; exit 0 }
    END { if (!found) exit 1 }
'
