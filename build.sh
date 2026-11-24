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
CONF_LTO=--disable-lto
# Multilib: enable/disable building of libraries for single or multiple motorola cpus.
CONF_MULTILIB=--enable-multilib
# Coldfire: enable/disable when multilib enabled.
CONF_COLDFIRE=disable
# Default cpu when using gcc.
CONF_DEFAULT_CPU=68000
# Toolchain install dir.
CONF_INSTALL=$HOME/toolchain

# Versions to download and build.
BINUTIL_VERSION="2.47"
GCC_VERSION="16.2.0"
#NEWLIB_VERSION="4.6.0.20260123"
NEWLIB_HASH="041b6410bfa86f8da4c3251f1df66c4a28a9b929"
ATARI_LIB_HASH="29c2d2f9379fe6114eacebcd0a5ac4578df3741f"

# Target specific settings
TARGET=m68k-atari-elf
PREFIX="$CONF_INSTALL/$TARGET"
PATH=$PATH:$PREFIX/bin
PATCHES="$PWD/patches"
#NEWLIB_PATH="$PWD/build/newlib-$NEWLIB_VERSION"
NEWLIB_PATH="$PWD/build/newlib-cygwin"

# Error handling.
# Any failing command stops the script. If that happens while a folder is being populated, the folder
# is renamed to "*.failed", so that the next run redoes that step instead of skipping it.
set -eE -o pipefail
STEP="Setup"
STEP_DIR=
FAILED_LINE=
FAILED_COMMAND=

# begin_step <description> [folder created by the step]
begin_step() {
	STEP="$1"
	STEP_DIR=
	echo "$STEP"
	if [ -n "$2" ]; then
		STEP_DIR="$PWD/$2"
		rm -rf "$STEP_DIR.failed"
	fi
}

end_step() {
	STEP="Between steps"
	STEP_DIR=
}

# Print an error message and exit.
die() {
	echo "ERROR: $*" >&2
	exit 1
}

on_exit() {
	local status=$?
	trap - ERR EXIT
	set +e
	if [ $status -eq 0 ]; then
		return
	fi
	echo >&2
	echo "ERROR: Build stopped (exit code $status) during step: $STEP" >&2
	if [ -n "$FAILED_LINE" ]; then
		echo "  build.sh line $FAILED_LINE: $FAILED_COMMAND" >&2
	fi
	if [ -n "$STEP_DIR" ] && [ -d "$STEP_DIR" ]; then
		cd "$(dirname "$STEP_DIR")"
		if mv "$STEP_DIR" "$STEP_DIR.failed"; then
			echo "  The incomplete folder was kept for inspection as: $STEP_DIR.failed" >&2
		else
			echo "  Delete this incomplete folder before running build.sh again: $STEP_DIR" >&2
		fi
	fi
	exit $status
}

trap 'FAILED_LINE=$LINENO; FAILED_COMMAND=$BASH_COMMAND' ERR
trap on_exit EXIT
trap 'STEP="$STEP (interrupted)"; exit 130' INT
trap 'STEP="$STEP (terminated)"; exit 143' TERM

# Check prerequisites.
if [ ! -d "$PATCHES" ]; then
	die "Folder not found: $PATCHES. Run build.sh from the m68k-atari-dev folder."
fi
for tool in curl git make patch tar gcc g++ python3; do
	if ! command -v $tool > /dev/null; then
		die "Required tool not found: $tool"
	fi
done


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
	begin_step "Downloading: binutils" "binutils-$BINUTIL_VERSION"
	curl --fail --location --output binutils-$BINUTIL_VERSION.tar.bz2 "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTIL_VERSION.tar.bz2"
	echo "Extracting: binutils"
	tar -xmf binutils-$BINUTIL_VERSION.tar.bz2
	end_step
fi

# Get gcc sources
if [ ! -d gcc-$GCC_VERSION ]; then
	begin_step "Downloading: gcc" "gcc-$GCC_VERSION"
	curl --fail --location --output gcc-$GCC_VERSION.tar.gz "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz"
	echo "Extracting: gcc"
	tar -xmf gcc-$GCC_VERSION.tar.gz
	cd gcc-$GCC_VERSION
	echo "Downloading: gcc prerequisites"
	./contrib/download_prerequisites
	echo "Patching: gcc"
	patch -p1 < $PATCHES/gcc/0001-m68k-Reject-moves-where-the-auto-inc-source-register.patch
	if [ "$CONF_COLDFIRE" == "disable" ]; then
		cp -f $PATCHES/gcc/t-atari gcc/config/m68k/t-atari
		patch gcc/config.gcc $PATCHES/gcc/config.gcc.diff
	fi
	cd ..
	end_step
fi

# Get newlib sources
#if [ ! -d newlib-$NEWLIB_VERSION ]; then
#	echo "Downloading: newlib"
#	curl --output newlib-$NEWLIB_VERSION.tar.gz "ftp://sourceware.org/pub/newlib/newlib-$NEWLIB_VERSION.tar.gz"
#	echo "Extracting: newlib"
#	tar -xmf newlib-$NEWLIB_VERSION.tar.gz
#	cd newlib-$NEWLIB_VERSION
#	# Fixing specs will hopefully be integrated in newlib in future.
#	yes | cp -rf $PATCHES/newlib/$SPECS_FILE libgloss/m68k/atari/atari-tos.specs
#	cd ..
#fi
if [ ! -d newlib-cygwin ]; then
	begin_step "Downloading: newlib" "newlib-cygwin"
	git clone https://sourceware.org/git/newlib-cygwin.git
	cd newlib-cygwin
	echo "Checking out: newlib hash $NEWLIB_HASH"
	git checkout $NEWLIB_HASH	
	echo "Patching: newlib"
	git apply $PATCHES/newlib/0001-m68k-atari-elf-Fixed-bug-for-missing-C-sections.patch
	git apply $PATCHES/newlib/0002-m68k-atari-elf-Fixed-bug-with-fstat-and-stat.patch
	git apply $PATCHES/newlib/0003-m68k-atari-elf-Fixed-bug-in-gettimeofday-where-the-t.patch
	git apply $PATCHES/newlib/0004-m68k-atari-elf-Fixed-several-bugs-with-the-argv-and-.patch
	git apply $PATCHES/newlib/0005-m68k-atari-elf-Fixed-bugs-in-isatty-and-fstat.patch
	git apply $PATCHES/newlib/0006-m68k-atari-elf-Fixed-bug-where-wrong-drive-could-be-.patch
	git apply $PATCHES/newlib/0007-m68k-atari-elf-Fixed-buffer-overrun.patch
	git apply $PATCHES/newlib/0008-m68k-atari-elf-Open-is-now-POSIX-compatible-and-a-bu.patch
	git apply $PATCHES/newlib/0009-m68k-atari-elf-GEM-error-codes-is-more-properly-mapp.patch
	git apply $PATCHES/newlib/0010-m68k-atari-elf-Added-memory-clobber-to-asm-calls.patch
	git apply $PATCHES/newlib/0011-m68k-atari-elf-Fixed-bug-with-line-endings.patch
	git apply $PATCHES/newlib/0012-m68k-atari-elf-Fixed-bug-with-C-destructors-not-hand.patch
	git apply $PATCHES/newlib/0013-m68k-atari-elf-Fixed-some-heap-and-stack-problems.patch
	git apply $PATCHES/newlib/0014-m68k-atari-elf-getentropy-is-now-a-lot-stronger.patch
	git apply $PATCHES/newlib/0015-m68k-atari-elf-Fixed-getmode-for-stderr.patch
	git apply $PATCHES/newlib/0016-m68k-atari-elf-Kill-is-properly-implemented-for-a-si.patch
	git apply $PATCHES/newlib/0017-m68k-atari-elf-Fixed-bug-where-O_APPEND-not-always-d.patch
	cd ..
	end_step
fi

# Get atari-libs sources
if [ ! -d atari-libs ]; then
	begin_step "Downloading: atari-libs" "atari-libs"
	git clone https://github.com/hildenborg/atari-libs.git
	# We checkout the specific stable commit.
	cd atari-libs
	#echo "Checking out: atari-libs hash $ATARI_LIB_HASH"
	#git checkout $ATARI_LIB_HASH
	cd ..
	end_step
fi

if [ "$machine" == "Mac" ]; then
	begin_step "Building: macos additionals"
	bash -e ../macos-additionals.sh $PREFIX $TARGET
	end_step
fi

# build binutils
if [ ! -d b-binutils ]; then
	begin_step "Building: binutils" "b-binutils"
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
	end_step
fi

# build gcc
if [ ! -d b-gcc ]; then
	begin_step "Building: gcc" "b-gcc"
	mkdir -p b-gcc
	cd b-gcc
	../gcc-$GCC_VERSION/configure --prefix=$PREFIX --target=$TARGET \
	$SYSTEM_SPECIFIC_FLAGS \
	--disable-libssp \
	--disable-nls \
	--enable-languages=c,c++ \
	--with-newlib \
	--disable-shared \
	--disable-threads \
	$CONF_LTO \
	$CONF_MULTILIB \
	--enable-target-optspace \
	--enable-sjlj-exceptions \
	--with-arch=m68k \
	--with-cpu=m$CONF_DEFAULT_CPU \
	--with-headers=$NEWLIB_PATH/newlib/libc/include
	make -j$BUILD_THREADS
	make install-strip
	rm -rf $PREFIX/$TARGET/sys-include
	
	# Check if we have built for lto (check if lto plugin exists).
	if [ -f $PREFIX/libexec/gcc/$TARGET/$GCC_VERSION/liblto_plugin.$SHARED_EXT ]; then
		# For some reason the lto plugin is not installed in all the correct places, so we fix that.	
		cp -f $PREFIX/libexec/gcc/$TARGET/$GCC_VERSION/liblto_plugin.$SHARED_EXT $PREFIX/lib/bfd-plugins/liblto_plugin.$SHARED_EXT
	fi

	cd ..
	end_step
fi

# build newlib
if [ ! -d b-newlib ]; then
	begin_step "Building: newlib" "b-newlib"
	mkdir -p b-newlib
	cd b-newlib
#	../newlib-$NEWLIB_VERSION/configure --prefix=$PREFIX --target=$TARGET \
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
	# We override the specs file with one that uses stuff outside of newlib.
	cp -f $PATCHES/newlib/$SPECS_FILE $PREFIX/lib/gcc/$TARGET/specs
	end_step
fi

# build atari-libs
if [ ! -d b-atari-libs ]; then
	begin_step "Building: atari-libs" "b-atari-libs"
	mkdir -p b-atari-libs
	cd atari-libs
	./build.sh $PREFIX $TARGET ../b-atari-libs $BUILD_THREADS
	cd ..
	end_step
fi

# Build m68k-atari-elf-prg (elf to prg converter)
if [ ! -d b-elf-prg ]; then
	begin_step "Building: elf to prg converter." "b-elf-prg"
	mkdir -p b-elf-prg
	cd b-elf-prg
	make -f ../../elf-prg/makefile -j$BUILD_THREADS M68K_TOOLKIT=$PREFIX all
	make -f ../../elf-prg/makefile M68K_TOOLKIT=$PREFIX install
	# The makefile does not report if the copying fails.
	if [ ! -f "$PREFIX/bin/m68k-atari-elf-prg" ]; then
		die "Not installed: $PREFIX/bin/m68k-atari-elf-prg"
	fi
	cd ..
	end_step
fi

# Build gdbserver
if [ ! -d b-gdbserver ]; then
	begin_step "Building: gdbserver." "b-gdbserver"
	mkdir -p b-gdbserver
	cd b-gdbserver
	make -f ../../gdbserver/makefile -j$BUILD_THREADS TOOLKIT=$PREFIX all
	make -f ../../gdbserver/makefile TOOLKIT=$PREFIX install
	# The makefile does not report if the copying fails.
	if [ ! -f "$PREFIX/bin/gdbsrv.ttp" ]; then
		die "Not installed: $PREFIX/bin/gdbsrv.ttp"
	fi
	cd ..
	end_step
fi

cd ..

# Done!
echo "Build finished: $PREFIX"

