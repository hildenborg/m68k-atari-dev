#!/bin/bash

#	Copyright (C) 2025 Mikael Hildenborg
#	SPDX-License-Identifier: MIT

# The built toolchain will not support wide characters or multi-treading, and will be optimized for size.
# A build folder will be created with individual "b-*" folders for separate parts.
# Any "b-*" can be deleted to force a rebuild of that specific part.
# The build folder can be deleted after build script have sucessfully finished.

# BUG NOTICE!
# There is a bug in gcc: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=87076
# This have the consequence that building this toolchain with:
# --enable-lto and --enable-multilib, will cause problems when selecting any other cpu than the default.
# So either disable lto, or disable multilib and select a default cpu that suits you.

# LTO: enable/disable link time optimizing.
CONF_LTO=--enable-lto
# Multilib: enable/disable building of libraries for single or multiple motorola cpus.
CONF_MULTILIB=--disable-multilib
# Default cpu when using gcc.
CONF_DEFAULT_CPU=68000
# Toolchain install dir.
CONF_INSTALL=$HOME/toolchain

# Target specific settings
TARGET=m68k-atari-elf
PREFIX="$CONF_INSTALL/$TARGET"
PATH=$PATH:$PREFIX/bin
PATCHES="$PWD/patches"
NEWLIB_PATH="$PWD/build/newlib-cygwin"

# Versions to download and build.
BINUTIL_VERSION="2.44"
GCC_VERSION="15.1.0"
NEWLIB_HASH="f13e8e21599ff12ea76980d6a8b19b0a5ebe9bed"

# Detect system
unameOut="$(uname -s)"
case "${unameOut}" in
    Linux*)     machine=Linux;;
    Darwin*)    machine=Mac;;
    CYGWIN*)    machine=Cygwin;;
    MINGW*)     machine=MinGw;;
    *)          machine="UNKNOWN:${unameOut}"
esac

# Set some system specific vars
BUILD_THREADS=1
SHARED_EXT="so"
SYSTEM_SPECIFIC_FLAGS=
if [ "$machine" == "Mac" ]; then
	BUILD_THREADS=$(sysctl -n hw.ncpu)
	SHARED_EXT="so"
	SYSTEM_SPECIFIC_FLAGS="--with-system-zlib"
elif [ "$machine" == "MinGw" ]; then
	BUILD_THREADS=$(nproc)
	SHARED_EXT="dll"
else
	BUILD_THREADS=$(nproc)
	SHARED_EXT="so"
fi

SPECS_FILE=specs
if [ "$CONF_LTO" == "--enable-lto" ]; then
SPECS_FILE=specs_lto
fi

# Make gcc toolchain build dir and enter
mkdir -p build
cd build

# Get binutils sources
if [ ! -d binutils-$BINUTIL_VERSION ]; then
	echo "Downloading: binutils"
	curl --output binutils-$BINUTIL_VERSION.tar.bz2 "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTIL_VERSION.tar.bz2"
	echo "Extracting: binutils"
	tar -xmf binutils-$BINUTIL_VERSION.tar.bz2
fi

# Get gcc sources
if [ ! -d gcc-$GCC_VERSION ]; then
	echo "Downloading: gcc"
	curl --output gcc-$GCC_VERSION.tar.gz "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz"
	echo "Extracting: gcc"
	tar -xmf gcc-$GCC_VERSION.tar.gz
	cd gcc-$GCC_VERSION
	echo "Downloading: gcc prerequisites"
	./contrib/download_prerequisites
	echo "Patching: gcc"
	yes | cp -rf $PATCHES/gcc-$GCC_VERSION/t-atari gcc/config/m68k/t-atari
	patch gcc/config.gcc $PATCHES/gcc-$GCC_VERSION/config.gcc.diff
	cd ..
fi

# Get newlib sources
if [ ! -d newlib-cygwin ]; then
	echo "Downloading: newlib"
	git clone https://sourceware.org/git/newlib-cygwin.git
	# We checkout the specific commit that our patch is made for.
	cd newlib-cygwin
	echo "Checking out: newlib hash $NEWLIB_HASH"
	git checkout $NEWLIB_HASH
	echo "Patching: newlib"
	git apply $PATCHES/newlib/0001-m68k-atari-elf-fixed-wrong-number-of-seconds-between.patch
	git apply $PATCHES/newlib/0002-m68k-atari-elf-changed-stack-and-heap-handling-to-mi.patch
	git apply $PATCHES/newlib/0003-m68k-atari-elf-updated-readme.patch
	git apply $PATCHES/newlib/0004-m68k-atari-elf-proper-setup-of-environment-variables.patch
	# Fixing specs will hopefully be integrated in newlib in future.
	yes | cp -rf $PATCHES/newlib/$SPECS_FILE libgloss/m68k/atari/atari-tos.specs
	cd ..
fi

if [ "$machine" == "Mac" ]; then
	../macos-additionals.sh $PREFIX $TARGET
fi

# build binutils
if [ ! -d b-binutils ]; then
	echo "Building: binutils"
	mkdir -p b-binutils
	cd b-binutils
	../binutils-$BINUTIL_VERSION/configure --prefix=$PREFIX --target=$TARGET \
	$SYSTEM_SPECIFIC_FLAGS \
	$CONF_LTO \
	$CONF_MULTILIB \
	--disable-nls \
	--disable-werror
	make MAKEINFO=true
	make install-strip MAKEINFO=true
	cd ..
fi

# build gcc
if [ ! -d b-gcc ]; then
	echo "Building: gcc"
	mkdir -p b-gcc
	cd b-gcc
	../gcc-$GCC_VERSION/configure --prefix=$PREFIX --target=$TARGET \
	$SYSTEM_SPECIFIC_FLAGS \
	--disable-libssp \
	--disable-nls \
	--enable-languages=c,c++ \
	--with-newlib \
	--disable-shared \
	--disable-thread \
	$CONF_LTO \
	$CONF_MULTILIB \
	--enable-target-optspace \
	--enable-sjlj-exceptions \
	--with-arch=m68k \
	--with-cpu=m$CONF_DEFAULT_CPU \
	--with-headers=$NEWLIB_PATH/newlib/libc/include
	make -j$BUILD_THREADS
	make install-strip
	# --with-headers copies headers to sys-include during gcc build but do not remove them after build, so we have to do it.
	# Thanks to Thorsten Otto for pointing that out.
	rm -rf $PREFIX/$TARGET/sys-include
	
	# Check if we have built for lto (check if lto plugin exists).
	if [ -f $PREFIX/libexec/gcc/$TARGET/$GCC_VERSION/liblto_plugin.$SHARED_EXT ]; then
		# For some reason the lto plugin is not installed in all the correct places, so we fix that.	
		yes | cp -rf $PREFIX/libexec/gcc/$TARGET/$GCC_VERSION/liblto_plugin.$SHARED_EXT $PREFIX/lib/bfd-plugins/liblto_plugin.$SHARED_EXT
	fi

	cd ..
fi

# build newlib
if [ ! -d b-newlib ]; then
	echo "Building: newlib"
	mkdir -p b-newlib
	cd b-newlib
	../newlib-cygwin/configure --prefix=$PREFIX --target=$TARGET \
	$SYSTEM_SPECIFIC_FLAGS \
	--with-float=soft \
	$CONF_LTO \
	--enable-target-optspace \
	--disable-newlib-wide-orient \
	--disable-newlib-mb \
	--disable-newlib-multithread \
	--enable-newlib-reent-small
	make -j$BUILD_THREADS
	make install
	cd ..

fi

# Build m68k-atari-elf-prg (elf to prg converter)
if [ ! -d b-elf-prg ]; then
	echo "Building: elf to prg converter."
	mkdir -p b-elf-prg
	cd b-elf-prg
	make -f ../../elf-prg/makefile -j$BUILD_THREADS M68K_TOOLKIT=$PREFIX all
	make -f ../../elf-prg/makefile M68K_TOOLKIT=$PREFIX install
	cd ..
fi

# Build gdbserver
if [ ! -d b-gdbserver ]; then
	echo "Building: gdbserver."
	mkdir -p b-gdbserver
	cd b-gdbserver
	make -f ../../gdbserver/makefile -j$BUILD_THREADS TOOLKIT=$PREFIX all
	make -f ../../gdbserver/makefile TOOLKIT=$PREFIX install
	cd ..
fi

cd ..

# Done!

