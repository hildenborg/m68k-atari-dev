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

// The maximum path length for the system.
// Legacy ST is 128, but we might need a zero at the end too.
#define MAX_PATH_LEN	130

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
