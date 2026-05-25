#	Copyright (C) 2026 Mikael Hildenborg
#	SPDX-License-Identifier: MIT

.DEFAULT_GOAL := all

TARGET_NAME := gdbstub
# ?= means that the value is only set if no previous value have been set.
# So it is possible to set another value from the command line.
BUILD_DIR ?= build
TOOLKIT	?= $${HOME}/toolchain/m68k-atari-elf

TOOLKIT_BIN	:= $(TOOLKIT)/bin
AS := $(TOOLKIT_BIN)/m68k-atari-elf-as
LD := $(TOOLKIT_BIN)/m68k-atari-elf-gcc
ELFTOPRG := $(TOOLKIT_BIN)/m68k-atari-elf-prg

ASFLAGS := -g --register-prefix-optional
# We use a specs file to avoid the default link script.
LDFLAGS := -specs=specs.spec -L$(BUILD_DIR)

TARGET_ELF := $(BUILD_DIR)/$(TARGET_NAME)
TARGET_PRG := $(TARGET_ELF).prg

# Make prg target
$(TARGET_PRG): $(TARGET_ELF)
	$(ELFTOPRG) $< $@

# Make elf target and specify our own link script
$(TARGET_ELF): $(BUILD_DIR)/start.o $(BUILD_DIR)/stub.o
	$(LD) $(LDFLAGS) $^ -o $@  -Xlinker -T -Xlinker link.ld

# assembly
$(BUILD_DIR)/start.o: start.s
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/stub.o: stub.s
	$(AS) $(ASFLAGS) -c $< -o $@

# Create build directory
$(BUILD_DIR): $(shell mkdir -p $(BUILD_DIR))

.PHONY: clean

all:	$(BUILD_DIR) $(TARGET_PRG)

clean:
	$(shell rm -r $(BUILD_DIR))

install:
	$(shell yes | cp -rf $(TARGET_PRG) $(TOOLKIT_BIN)/$(TARGET_NAME).prg)
	
