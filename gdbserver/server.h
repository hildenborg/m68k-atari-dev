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

extern unsigned int Cookie_CPU;
extern unsigned int Cookie_VDO;
extern unsigned int Cookie_FPU;
extern unsigned int Cookie_MCH;


int ServerMain(int argc, char** argv);
void ServerCommandLoop(int si_signo, int si_code);

// Helper macros for debugging pursposes
#define BREAKPOINT_OP asm ("trap #0");
#define ILLEGAL_OP asm ("illegal");

#ifdef __cplusplus
}
#endif

#endif // DBG_DEFINED
