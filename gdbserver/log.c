/*
	Copyright (C) 2026 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#include "log.h"
#include "hex.h"
#include "bios_calls.h"
#include "clib.h"

const char newline[] = "\r\n";

bool			log_debug = false;				// cmdline option --debug
bool			log_debug_remote = false;		// cmdline option --debug-remote

#ifndef NO_CON_OR_LOG

int 			logHandle;

void InitLog(int argc, char** argv)
{
	logHandle = -1;
	if (argc >= 2 && StringCompare("--log", argv[1]) > 0)
	{
		log_debug_remote = true;
		log_debug = true;
		logHandle = Fcreate("gdblog.txt", 0);
	}
}

void ExitLog(void)
{
	if (logHandle > 0)
	{
		Fclose((unsigned short)logHandle);
		log_debug_remote = false;
		log_debug = false;
	}
}

void ConOut(const char* txt)
{
	if (logHandle > 0)
	{
		Fwrite((unsigned short)logHandle, strlen(txt), txt);
	}
	else
	{
		int len = 0;
		while (txt[len] != 0) 
		{
			Bconout(DEV_CONSOLE, txt[len]);
			++len;
		}
	}
}

void DbgOut(const char* txt)
{
#ifndef DEBUG_OPTIONS_ON
	if (log_debug)
#endif
	{
		ConOut(txt);
	}
}

void DbgRemOut(const char* txt)
{
#ifndef DEBUG_OPTIONS_ON
	if (log_debug_remote)
#endif
	{
		ConOut(txt);
	}
}

void ConOutVal(const char* name, unsigned int val)
{
	char buf[12];
	int i = 8;
	buf[i] = 0;
	while (--i >= 0)
	{
		buf[i] = NibbleToHex(val);
		val = val >> 4;
	}
	ConOut("\t");
	ConOut(name);
	ConOut(": 0x");
	ConOut(buf);
	ConOut(newline);
}

void DbgOutVal(const char* name, unsigned int val)
{
#ifndef DEBUG_OPTIONS_ON
	if (log_debug)
#endif
	{
		ConOutVal(name, val);
	}
}

void DbgRemOutVal(const char* name, unsigned int val)
{
#ifndef DEBUG_OPTIONS_ON
	if (log_debug_remote)
#endif
	{
		ConOutVal(name, val);
	}
}

#endif // NO_CON_OR_LOG
