#!/bin/bash

source edksetup.sh
build -a AARCH64 -t GCC5 -p MyLoader/loader/MyLoader.dsc -b DEBUG
