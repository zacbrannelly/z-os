#!/bin/bash

# Build object file for the kernel
gcc \
  -c MyLoader/kernel/kernel.c \
  -o Build/MyLoader/kernel.o \
  -ffreestanding \
  -fno-builtin \
  -fno-stack-protector \
  -O0 \
  -g

# Link the object file into an executable ELF file
ld -T MyLoader/kernel/linker.ld -o Build/MyLoader/kernel.elf Build/MyLoader/kernel.o
