/*
	Copyright (C) 2026 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#ifndef LOG_DEFINED
#define LOG_DEFINED

#include <stdbool.h>

// Disable all console or logfile output (saves about 3kb)
//#define NO_CON_OR_LOG

// Set --debug and --remote-debug to active.
//#define DEBUG_OPTIONS_ON

extern const char newline[];
extern bool			log_debug;				// cmdline option --debug
extern bool			log_debug_remote;		// cmdline option --debug-remote

#ifdef NO_CON_OR_LOG
#define InitLog(argc, argv)
#define ExitLog()
#define ConOut(txt)
#define DbgOut(txt)
#define DbgRemOut(txt)
#define ConOutVal(name, val)
#define DbgOutVal(name, val)
#define DbgRemOutVal(name, val)
#else // NO_CON_OR_LOG
void InitLog(int argc, char** argv);
void ExitLog(void);
void ConOut(const char* txt);
void DbgOut(const char* txt);
void DbgRemOut(const char* txt);
void ConOutVal(const char* name, unsigned int val);
void DbgOutVal(const char* name, unsigned int val);
void DbgRemOutVal(const char* name, unsigned int val);
#endif // NO_CON_OR_LOG

#endif // LOG_DEFINED
