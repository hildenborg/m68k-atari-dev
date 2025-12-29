#	Copyright (C) 2025 Mikael Hildenborg
#	SPDX-License-Identifier: MIT

.DEFAULT_GOAL := all

# Project settings
BUILD_DIR := build
SRC_DIR := .
CFLAGS := -O0 -g -Wall	# -O0 so it is easier to debug.
LDFLAGS :=

# Toolchain command settings
TOOLCHAIN_BIN := $(TOOLCHAIN_M68K)/bin
CC := $(TOOLCHAIN_BIN)/m68k-atari-mintelf-gcc
LD := $(TOOLCHAIN_BIN)/m68k-atari-mintelf-gcc

# Targets
TARGET_ELF := $(BUILD_DIR)/$(TARGET_NAME)

# Compile
$(BUILD_DIR)/hello.o: hello.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build elf executable
$(TARGET_ELF): $(BUILD_DIR)/hello.o
	$(LD) $(LDFLAGS) $^ -o $@

# Create build directory
$(BUILD_DIR): $(shell mkdir -p $(BUILD_DIR))

all:	$(BUILD_DIR) $(TARGET_ELF)

clean:
	$(shell rm -r $(BUILD_DIR))

