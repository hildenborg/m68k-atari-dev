#!/bin/bash

#	Copyright (C) 2025 Mikael Hildenborg
#	SPDX-License-Identifier: MIT

# You need to set up Hatari with a tos image before calling this script.
# You also need a folder to use as a gem drive.

# This is the extension less name of your executable.
export TARGET_NAME=hello

# Set this to where your Hatari ST gem drive is.
export ST_HD_PATH=$HOME/sthd

# Set this to where your toolchain is.
export TOOLCHAIN_M68K=$HOME/toolchain/m68k-atari-elf

# These two lines are the local socket port and the file handler used to connect gdb and "gdbsrv.ttp".
export GDB_COM_PORT=$HOME/st_a
export SRV_COM_PORT=$HOME/st_b

# Make virtual serial ports using socat
socat PTY,link=$GDB_COM_PORT,raw,echo=0 PTY,link=$SRV_COM_PORT,raw,echo=0 & disown
SOCAT_PID=$!

# Start VsCode
code .

# Start Hatari with SCC port set
hatari --cpulevel 3 --machine falcon --memsize 14 --fpu 68882 -d $ST_HD_PATH --gemdos-drive C --scc-b-in $SRV_COM_PORT --scc-b-out $SRV_COM_PORT

#Make sure socat is shut down when we exit.
kill $SOCAT_PID

