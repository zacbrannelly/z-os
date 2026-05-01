#!/bin/bash

source edksetup.sh
build -a AARCH64 -t GCC5 -p bootloader/loader/bootloader.dsc -b DEBUG
