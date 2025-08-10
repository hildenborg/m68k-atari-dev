#!/bin/bash

# We are in the build folder when this is executed.

PREFIX=$1
TARGET=$2
BUILD_THREADS=$(sysctl -n hw.ncpu)

SOCAT_VERSION="1.7.4.4"
ZLIB_VERSION="1.3.1"
GDB_VERSION="16.3"

# Mac os needs zlib to be able to build binutils correct
if [[ ! "$(( gcc -lz) 2>&1)" =~ "_main" ]]; then 
	if [ ! -d zlib-$ZLIB_VERSION ]; then
		echo "Downloading: zlib"
		curl --output zlib-$ZLIB_VERSION.tar.gz https://zlib.net/zlib-$ZLIB_VERSION.tar.gz
		echo "Extracting: zlib"
		tar -xmf zlib-$ZLIB_VERSION.tar.gz
	fi
	if [ ! -d b-zlib ]; then
		echo "Building: zlib"
		mkdir -p b-zlib
		cd b-zlib
		../zlib-$ZLIB_VERSION/configure
		make -j$BUILD_THREADS
		make install
		cd ..
	fi
fi

# Mac os does not have socat that we use for connecting hatari and gdb.
# So we check for its existance and if non existing, downloads and builds it.
if [ -z $(which socat) ]; then 
	if [ ! -d socat-$SOCAT_VERSION ]; then
		echo "Downloading: socat"
		curl --output socat-$SOCAT_VERSION.tar.gz "http://www.dest-unreach.org/socat/download/socat-$SOCAT_VERSION.tar.gz"
		echo "Extracting: socat"
		tar -xmf socat-$SOCAT_VERSION.tar.gz
	fi
	if [ ! -d b-socat ]; then
		echo "Building: socat"
		mkdir -p b-socat
		cd b-socat
		../socat-$SOCAT_VERSION/configure
		make -j$BUILD_THREADS
		make install
		cd ..
	fi
fi

# Building gdb fails on mac.
# Someone who actually knows and uses mac should have a go at it.
#
#if [ -z $(which gdb-multiarch) ]; then 
#	if [ ! -d gdb-$GDB_VERSION ]; then
#		echo "Downloading: gdb"
#		curl --output gdb-$GDB_VERSION.tar.xz "https://ftp.gnu.org/gnu/gdb/gdb-$GDB_VERSION.tar.xz"
#		echo "Extracting: gdb"
#		tar -xmf gdb-$GDB_VERSION.tar.xz
#	fi
#	if [ ! -d b-gdb ]; then
#		echo "Building: gdb"
#		mkdir -p b-gdb
#		cd b-gdb
#		../gdb-$GDB_VERSION/configure --enable-targets=all \
#			--disable-debug \
#			--disable-dependency-tracking \
#			--enable-tui
#		
#		make -j$BUILD_THREADS all-gdb
#		make install-gdb
#		cd ..
#	fi
#fi

