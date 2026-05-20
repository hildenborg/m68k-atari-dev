#!/bin/bash

CONF_INSTALL=$HOME/toolchain
TARGET=m68k-atari-elf
PREFIX="$CONF_INSTALL/$TARGET"

rm -rf build
mkdir -p build
cd build
make -f ../makefile TOOLKIT=$PREFIX all
cp gdbsrv.ttp $HOME/sthd/gdbsrv.ttp
cd ..
