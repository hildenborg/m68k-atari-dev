/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#ifndef SERVER_DEFINED
#define SERVER_DEFINED

#include "gem_basepage.h"

#ifdef __cplusplus
extern "C" {
#endif

// The maximum path length for the system.
// Legacy ST is 128, but we might need a zero at the end too.
#define MAX_PATH_LEN	130

// Max number of argument characters in a GEMDOS command line (excluding length byte and terminator).
#define MAX_CMDLINE_LEN	125

extern char com_method[];
extern bool option_multi;
extern bool run_once;

int ServerMain(bool loadRequest);
void ServerCommandLoop(int si_signo, int si_code);
// Returns -1 if the user pressed the key that quits the server.
int CheckServerQuitKey(void);

#ifdef __cplusplus
}
#endif

#endif // SERVER_DEFINED
