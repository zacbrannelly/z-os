#!/bin/bash

export ZOS_RUN_TESTS=ON

./scripts/fetch_qemu_efi.sh
./build.sh
./scripts/run_qemu.sh
