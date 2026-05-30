/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#ifndef DBG_DEFINED
#define DBG_DEFINED

#include "gem_basepage.h"

#ifdef __cplusplus
extern "C" {
#endif

extern char inferior_filename[];
extern char inferior_cmdline[];
extern char inferior_workpath[];
extern char com_method[];
extern bool option_multi;
extern bool run_once;

int ServerMain(bool loadRequest);
void ServerCommandLoop(int si_signo, int si_code);

// Helper macros for debugging pursposes
#define BREAKPOINT_OP asm ("trap #0");
#define ILLEGAL_OP asm ("illegal");

#ifdef __cplusplus
}
#endif

#endif // DBG_DEFINED
