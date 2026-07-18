.DEFAULT_GOAL := all

# Project settings
BUILD_DIR := build
CPU := 68000
TOOLKIT	?= /usr/local/bin/m68k-atari-elf

LDFLAGS :=
CFLAGS := -Wall -Os -g -m$(CPU)
ASFLAGS := -g --register-prefix-optional -mcpu=$(CPU)

SOURCES := main.c main_asm.s hw_detect.s

# Toolchain command settings
# Project build architecture settings
TOOLKIT_BIN := $(TOOLKIT)/bin
CC := $(TOOLKIT_BIN)/m68k-atari-elf-gcc
AS := $(TOOLKIT_BIN)/m68k-atari-elf-as
LD := $(TOOLKIT_BIN)/m68k-atari-elf-gcc
ELFTOPRG := $(TOOLKIT_BIN)/m68k-atari-elf-prg

# Building object list
OBJS := $(foreach source,$(SOURCES),$(BUILD_DIR)/$(basename $(source)).o)

# Targets
TARGET_PRG := $(BUILD_DIR)/$(TARGET_NAME).prg
TARGET_ELF := $(BUILD_DIR)/$(TARGET_NAME)

# asm
$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) -c $< -o $@

# c
$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build target
$(TARGET_PRG): $(TARGET_ELF)
	$(ELFTOPRG) $< $@

$(TARGET_ELF): $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@ -ltos

# Create build directory
$(BUILD_DIR): $(shell mkdir -p $(BUILD_DIR))

all:	$(BUILD_DIR) $(TARGET_PRG)

clean:
	$(shell rm -r $(BUILD_DIR))

