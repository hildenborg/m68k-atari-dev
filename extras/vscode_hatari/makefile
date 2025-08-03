.DEFAULT_GOAL := all

# Project settings
BUILD_DIR := build
SRC_DIR := .
CFLAGS := -g -Wall
LDFLAGS :=

# Toolchain command settings
TOOLCHAIN_BIN := $(TOOLCHAIN_M68K)/bin
CC := $(TOOLCHAIN_BIN)/m68k-atari-elf-gcc
LD := $(TOOLCHAIN_BIN)/m68k-atari-elf-gcc
ELFTOPRG := $(TOOLCHAIN_BIN)/m68k-atari-elf-prg

# Targets
TARGET_PRG := $(BUILD_DIR)/$(TARGET_NAME).prg
TARGET_ELF := $(BUILD_DIR)/$(TARGET_NAME)

# Compile
$(BUILD_DIR)/hello.o: hello.c
	$(CC) $(CFLAGS) -c $< -o $@

# Convert elf to prg
$(TARGET_PRG): $(TARGET_ELF)
	$(ELFTOPRG) $< $@

# Build elf executable
$(TARGET_ELF): $(BUILD_DIR)/hello.o
	$(LD) $(LDFLAGS) $^ -o $@

# Create build directory
$(BUILD_DIR): $(shell mkdir -p $(BUILD_DIR))

all:	$(BUILD_DIR) $(TARGET_PRG)

clean:
	$(shell rm -r $(BUILD_DIR))

