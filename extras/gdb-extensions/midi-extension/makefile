#	Copyright (C) 2026 Mikael Hildenborg
#	SPDX-License-Identifier: MIT

.DEFAULT_GOAL := all

TARGET_NAME := gdbmidi
BUILD_DIR ?= build

TOOLKIT	?= $${HOME}/toolchain/m68k-atari-elf

TOOLKIT_BIN	:= $(TOOLKIT)/bin
AS := $(TOOLKIT_BIN)/m68k-atari-elf-as
LD := $(TOOLKIT_BIN)/m68k-atari-elf-gcc
ELFTOPRG := $(TOOLKIT_BIN)/m68k-atari-elf-prg

ASFLAGS := -g --register-prefix-optional
LDFLAGS := -specs=specs.spec -L$(BUILD_DIR)

TARGET_ELF := $(BUILD_DIR)/$(TARGET_NAME)
TARGET_PRG := $(TARGET_ELF).prg

# Make prg target
$(TARGET_PRG): $(TARGET_ELF)
	$(ELFTOPRG) $< $@

# Make elf target
$(TARGET_ELF): $(BUILD_DIR)/start.o $(BUILD_DIR)/midi.o
	$(LD) $(LDFLAGS) $^ -o $@  -Xlinker -T -Xlinker link.ld

# assembly
$(BUILD_DIR)/start.o: start.s
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/midi.o: midi.s
	$(AS) $(ASFLAGS) -c $< -o $@

# Create build directory
$(BUILD_DIR): $(shell mkdir -p $(BUILD_DIR))

.PHONY: clean

all:	$(BUILD_DIR) $(TARGET_PRG)

clean:
	$(shell rm -r $(BUILD_DIR))

install:
	$(shell yes | cp -rf $(TARGET_PRG) $(TOOLKIT_BIN)/$(TARGET_NAME).prg)
	
