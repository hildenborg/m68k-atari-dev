#!/bin/bash

#	Copyright (C) 2026 Mikael Hildenborg
#	SPDX-License-Identifier: MIT

# You need to set up Hatari with a tos image before calling this script.
# You also need a folder to use as a gem drive.

# This is the extension less name of your executable.
export TARGET_NAME=hello

# Set this to where your toolchain is.
export TOOLCHAIN_M68K=$HOME/toolchain/m68k-atari-elf

# The port for your RS232 connection to the Atari computer.
# On linux this is (usually) /dev/ttyUSB0
export GDB_COM_PORT=/dev/ttyUSB0

# Start VsCode
code .

