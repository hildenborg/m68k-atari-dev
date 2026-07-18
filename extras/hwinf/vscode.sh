#!/bin/bash

export TARGET_NAME=hwinf

export ST_HD_PATH=$HOME/sthd

export TOOLKIT=$HOME/toolchain/m68k-atari-elf

export GDB_COM_PORT=$HOME/st_a
export SRV_COM_PORT=$HOME/st_b

socat PTY,link=$GDB_COM_PORT,raw,echo=0 PTY,link=$SRV_COM_PORT,raw,echo=0 & disown
SOCAT_PID=$!

code .

hatari --machine ste --memsize 4 -d $ST_HD_PATH --gemdos-drive C --rs232-in $SRV_COM_PORT --rs232-out $SRV_COM_PORT

kill $SOCAT_PID
