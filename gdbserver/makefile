
.DEFAULT_GOAL := all

TARGET_NAME := gdbsrv
BUILD_DIR := .
SRC_DIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
SOURCES := main.c server.c exceptions_asm.s exceptions.c bios_calls.c target_xml.c

# Project build architecture settings
CPU := 68000
TOOLKIT	?= /usr/local/bin/m68k-atari-elf

# Use m68k-atari-elf toolchain
# Toolkit executables, libraries and directories settings
TOOLKIT_BIN	:= $(TOOLKIT)/bin
CC := $(TOOLKIT_BIN)/m68k-atari-elf-gcc
AS := $(TOOLKIT_BIN)/m68k-atari-elf-as
LD := $(TOOLKIT_BIN)/m68k-atari-elf-gcc
ELFTOPRG := $(TOOLKIT_BIN)/m68k-atari-elf-prg

CFLAGS := -Wall -Os -g -m$(CPU) -flto
ASFLAGS := -g --register-prefix-optional -mcpu=$(CPU)
LDFLAGS := 

TARGET_PRG := $(BUILD_DIR)/$(TARGET_NAME).ttp

# Defining target name
TARGET_ELF := $(BUILD_DIR)/$(TARGET_NAME)

# Building object list
OBJS := $(foreach source,$(SOURCES),$(BUILD_DIR)/$(basename $(source)).o)

# Make prg target
$(TARGET_PRG): $(TARGET_ELF)
	$(ELFTOPRG) $< $@

# Make elf target
$(TARGET_ELF): $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

# assembly
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s
	$(AS) $(ASFLAGS) -c $< -o $@

# c source
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directory
$(BUILD_DIR): $(shell mkdir -p $(BUILD_DIR))

.PHONY: clean

all:	$(BUILD_DIR) $(TARGET_PRG)

clean:
	$(shell rm -r $(BUILD_DIR))

install:
	$(shell yes | cp -rf $(TARGET_PRG) $(TOOLKIT_BIN)/$(TARGET_NAME).ttp)
	
