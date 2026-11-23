# Reproduces a GCC 16.1 m68k code generation bug. See README.md.
#
#   make        build build/gcc_bug.prg
#   make check  look for the bad instruction in the generated assembly
#   make run    run the program in Hatari
#   make dumps  show the RTL before and after the late_combine2 pass

.DEFAULT_GOAL := all

TOOLKIT ?= $(HOME)/toolchain/m68k-atari-elf
PREFIX := $(TOOLKIT)/bin/m68k-atari-elf-
CC := $(PREFIX)gcc
ELFTOPRG := $(PREFIX)prg
HATARI ?= hatari
HATARI_FLAGS ?= --machine ste --memsize 4
# A TOS image for Hatari (optional, if Hatari has no default)
TOS ?=

BUILD_DIR := build
BUG_FLAGS := -O2
OK_FLAGS := -O2 -fno-late-combine-instructions

# Option sets tried by "make check"
CHECK_FLAGS := "-O1" "-Os" "-O2" "-O3" "-O2 -mshort" "-O2 -m68020" "-O2 -m68030" \
	"-O2 -m68040" "-O2 -m68060" "-O2 -mlra" "-O1 -fivopts -flate-combine-instructions" \
	"-O2 -fno-late-combine-instructions" "-O2 -fno-auto-inc-dec" "-O2 -fno-ivopts"

# (aN)+ as source, with the same aN in the address of the destination
BAD_SED := /move\.[bwl] (\(%a[0-7]\))+,[-0-9]*(\1[,)]/p

.PHONY: all check run dumps clean

all: $(BUILD_DIR)/gcc_bug.prg

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/copy_bug.o: copy.c makefile | $(BUILD_DIR)
	$(CC) $(BUG_FLAGS) -DCOPY_NAME=copy_bug -c $< -o $@

$(BUILD_DIR)/copy_ok.o: copy.c makefile | $(BUILD_DIR)
	$(CC) $(OK_FLAGS) -DCOPY_NAME=copy_ok -c $< -o $@

$(BUILD_DIR)/main.o: main.c makefile | $(BUILD_DIR)
	$(CC) -Wall -O1 -c $< -o $@

$(BUILD_DIR)/gcc_bug.elf: $(BUILD_DIR)/main.o $(BUILD_DIR)/copy_bug.o $(BUILD_DIR)/copy_ok.o
	$(CC) $^ -ltos -o $@

$(BUILD_DIR)/gcc_bug.prg: $(BUILD_DIR)/gcc_bug.elf
	$(ELFTOPRG) $< $@

check:
	@$(CC) --version | head -n 1
	@for f in $(CHECK_FLAGS); do \
		asm=$$($(CC) $$f -S copy.c -o -) || { echo "ERROR  $$f"; exit 1; }; \
		bad=$$(printf '%s\n' "$$asm" | sed -n '$(BAD_SED)'); \
		if [ -n "$$bad" ]; then echo "BAD    $$f  ->$$bad"; else echo "ok     $$f"; fi; \
	done

run: $(BUILD_DIR)/gcc_bug.prg
	$(HATARI) $(if $(TOS),--tos $(TOS)) $(HATARI_FLAGS) $<

dumps: | $(BUILD_DIR)
	$(CC) $(BUG_FLAGS) -S copy.c -o $(BUILD_DIR)/copy.s \
		-fdump-rtl-postreload=$(BUILD_DIR)/before.rtl \
		-fdump-rtl-late_combine2-details=$(BUILD_DIR)/after.rtl
	@echo "=== before late_combine2"; sed -n '/^(insn.*/,/^$$/p' $(BUILD_DIR)/before.rtl | sed -n '/plus:SI (reg:SI 8/,/post_inc/p' | head -n 12
	@echo "=== late_combine2"; sed -n '/trying to combine definition of r9/,/^$$/p' $(BUILD_DIR)/after.rtl
	@echo "=== generated code"; sed -n '/^copy:/,/rts/p' $(BUILD_DIR)/copy.s

clean:
	rm -rf $(BUILD_DIR)
