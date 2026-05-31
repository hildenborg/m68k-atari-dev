/*
	Copyright (C) 2026 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#ifndef INFERIOR_DEFINED
#define INFERIOR_DEFINED

#include "gem_basepage.h"

typedef enum
{
	NOT_LOADED,
	LOADED,
	RUNNING
} InferiorState;

extern InferiorState inferiorState;
extern struct BasePage* inferiorBasePage;
extern bool inferior_is_mintelf;
extern char inferior_filename[];
extern char inferior_cmdline[];
extern char inferior_workpath[];

int LoadInferior(const char* fileName, const char* cmdLine, const char* environment);
bool RunInferior(int* return_code);
void TerminateInferior(int si_signo);
bool IsInferiorStartBreak(int si_signo, int si_code);
void ClearInferiorStartBreak(void);

#endif // INFERIOR_DEFINED
