#!/bin/bash

#	Copyright (C) 2025 Mikael Hildenborg
#	SPDX-License-Identifier: MIT

TARGET_BIN=$HOME/toolchain/m68k-atari-elf/bin

# Compile and link elf executable (default cpu is 68000)
$TARGET_BIN/m68k-atari-elf-gcc -o hello hello.c

# Convert elf to atari prg.
$TARGET_BIN/m68k-atari-elf-prg hello hello.prg

