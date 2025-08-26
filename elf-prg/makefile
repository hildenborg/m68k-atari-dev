#	Copyright (C) 2025 Mikael Hildenborg
#	SPDX-License-Identifier: MIT

.DEFAULT_GOAL := all

TARGET_NAME := m68k-atari-elf-prg
BUILD_DIR := .
SRC_DIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
SOURCES := main.c

M68K_TOOLKIT	?= /usr/local/bin/m68k-atari-elf
M68K_TOOLKIT_BIN	:= $(M68K_TOOLKIT)/bin

# Compiler and linker flags
CFLAGS := -Wall -O2
LDFLAGS := 

# Defining target name
TARGET := $(BUILD_DIR)/$(TARGET_NAME)

# Building object list
OBJS := $(foreach source,$(SOURCES),$(BUILD_DIR)/$(basename $(source)).o)

# Make elf target
$(TARGET): $(OBJS)
	gcc $(LDFLAGS) $^ -o $@

# c source
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	gcc $(CFLAGS) -I $(M68K_TOOLKIT) -I $(M68K_TOOLKIT)/m68k-atari-elf/include -c $< -o $@

.PHONY: all
all: $(TARGET)

install:
	$(shell mkdir -p $(M68K_TOOLKIT_BIN))
	$(shell yes | cp -rf $(BUILD_DIR)/$(TARGET_NAME) $(M68K_TOOLKIT_BIN)/$(TARGET_NAME))

