#!/bin/bash

./scripts/fetch_qemu_efi.sh
./build.sh
./scripts/run_qemu.sh
