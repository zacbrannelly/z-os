#!/bin/bash

./scripts/fetch_qemu_efi.sh
./dbg_build.sh
./scripts/debug_run_qemu.sh
