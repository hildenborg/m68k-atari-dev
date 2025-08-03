#!/bin/bash

TARGET_BIN=$HOME/toolchain/m68k-atari-elf/bin
SDIR=$PWD

mkdir -p build
cd build

# Create temporary links and add to path so gcc and ar points to our toolchain.
ln -s $TARGET_BIN/m68k-atari-elf-gcc $SDIR/build/gcc
ln -s $TARGET_BIN/m68k-atari-elf-ar $SDIR/build/ar
PATH=$SDIR/build:$PATH:$TARGET_BIN

# Download and unpack lua
curl --output lua-5.4.8.tar.gz "https://www.lua.org/ftp/lua-5.4.8.tar.gz"
tar -xmf lua-5.4.8.tar.gz

cd lua-5.4.8
# Apply patches that sets correct build flags
patch Makefile $SDIR/makefile.diff
patch src/Makefile $SDIR/src_makefile.diff
yes | cp -rf $SDIR/stack.c src/stack.c

# Build lua
make

# Convert lua elf to lua ttp
$TARGET_BIN/m68k-atari-elf-prg src/lua src/lua.ttp

# Step out of lua-5.4.8
cd ..

# Remove temporary links
rm $SDIR/build/gcc
rm $SDIR/build/ar

# Step out of build
cd ..

echo "Building lua done. 'lua.ttp' can be found at: build/lua-5.4.8/src/lua.ttp"

