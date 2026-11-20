/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

/*
	This is not gdbserver.
	It is something that partly works similarily to gdbserver.
	The usage is similar enough to warrant the name gdbserver.

	We do not use any standard libraries or c runtimes in this code.
	Only pure atari code here to limit the effect this server has on the system.
	And also limit the size. 
	A smaller gdbserver allows us to debug larger programs.
*/

#include "server.h"
#include "log.h"
#include "clib.h"
#include "inferior.h"
#include "comm.h"

/*
	Option handling for this server is made to follow the real gdbserver documentation.
	However, most of the options for the real gdbserver is not applicable for us.

	gdbsrv.ttp	[options] [comm] prog [args]

	comm	(Default if missing: AUX, or SDBG if a plugin is installed)
		Starts with AUX, COM or SDBG, and has no '.', so it can't be confused with a program name.
	prog	(Default if missing: turns on option --multi)
		The executable you want to debug.
	args
		Commandline arguments for prog.
	options:
		--log
			Must be the first argument. Turns on --debug and --debug-remote,
			and writes the output to gdblog.txt instead of the console.
		--debug
			Outputs status information to console.
		--debug-remote
			Outputs remote package communication to console.
		--multi
			Starts the server without any executable to debug.
			Waits for gdb to connect with target extended-remote and tell us what executable to debug.
		--once
			In extended mode, exits the server when the inferior is killed.
		--baud=rate
			Serial speed. Default 19200. One of the rates TOS Rsconf supports:
			19200, 9600, 4800, 3600, 2400, 2000, 1800, 1200, 600, 300, 200, 150, 134, 110, 75, 50.
*/

// Reads a decimal number. Returns false if str is empty, has other characters than digits, or is too long.
static bool ReadDecimal(const char* str, unsigned int* value)
{
	unsigned int v = 0;
	short digits = 0;
	for (; *str != 0; ++str, ++digits)
	{
		if (*str < '0' || *str > '9' || digits >= 6)
		{
			return false;
		}
		v = (v << 3) + (v << 1) + (unsigned int)(*str - '0');	// v * 10 without a 32 bit multiplication.
	}
	*value = v;
	return digits > 0;
}
static bool IsCommName(const char* arg)
{
	for (const char* p = arg; *p != 0; ++p)
	{
		if (*p == '.')
		{
			return false;	// Program names have an extension.
		}
	}
	return StringCompare("AUX", arg) > 0 || StringCompare("COM", arg) > 0 || StringCompare("SDBG", arg) > 0;
}

int HandleOptions(int argc, char** argv)
{
	int result = 0;
	short nameEnd;
	bool loadRequest = false;
	com_method[0] = 0;
	char *inferior_args = inferior_cmdline + 1;
	inferior_cmdline[0] = 0;
	inferior_cmdline[1] = 0;
	inferior_filename[0] = 0;
	if (argc <= 1)
	{
		DbgOut("\tNo args.\r\n");
		return 0;
	}
	for (int i = 1; i < argc; ++i)
	{
		if (loadRequest)
		{
			inferior_args = StrCopy(argv[i], inferior_args);
			*inferior_args++ = ' ';
			DbgOut("Inferior arg: ");
			DbgOut(argv[i]);
			DbgOut(newline);
		}
		else
		{
			if (StringCompare("--", argv[i]) > 0)
			{
				if (StringCompare("--multi", argv[i]) > 0)
				{
					// If extended mode is not requested, then this option stops gdbsrv from exiting after killing inferior.
					option_multi = true;
					DbgOut("Using: --multi\r\n");
				}
				else if (StringCompare("--once", argv[i]) > 0)
				{
					run_once = true;
					DbgOut("Using: --once\r\n");
				}
				else if ((nameEnd = StringCompare("--baud=", argv[i])) > 0)
				{
					unsigned int baud;
					if (!ReadDecimal(argv[i] + nameEnd, &baud) || !SetSerialBaud(baud))
					{
						ConOut("Unsupported baud rate: ");
						ConOut(argv[i] + nameEnd);
						ConOut(newline);
						result = -1;
					}
					else
					{
						DbgOut("Using: ");
						DbgOut(argv[i]);
						DbgOut(newline);
					}
				}
#ifndef NO_CON_OR_LOG
				else if (StringCompare("--debug-remote", argv[i]) > 0)
				{
					log_debug_remote = true;
					DbgOut("Using: --debug-remote\r\n");
				}
				else if (StringCompare("--debug", argv[i]) > 0)
				{
					log_debug = true;
					DbgOut("Using: --debug\r\n");
				}
				else if (StringCompare("--log", argv[i]) > 0)
				{
					DbgOut("Using: --log\r\n");
				}
#endif // NO_CON_OR_LOG
				else
				{
					ConOut("Unknown option: ");
					ConOut(argv[i]);
					ConOut(newline);
					result = -1;
				}
			}
			else if (IsCommName(argv[i]))
			{
				StrCopy(argv[i], com_method);
				DbgOut("Using connection: ");
				DbgOut(argv[i]);
				DbgOut(newline);
			}
			else
			{
				StrCopy(argv[i], inferior_filename);
				loadRequest = true;
				DbgOut("Inferior file: ");
				DbgOut(argv[i]);
				DbgOut(newline);
			}
		}
	}
	if (option_multi)
	{
		loadRequest = false;	// Never load inferior directly when multi.
	}
	if (inferior_args[-1] != 0)
	{
		inferior_args[-1] = 0;
		inferior_cmdline[0] = (char)strlen(&inferior_cmdline[1]);		
	}
	if (result == 0 && loadRequest)
	{
		result = 1;
	}
	return result;
}


int main(int argc, char** argv)
{
	InitLog(argc, argv);
	int result = HandleOptions(argc, argv);
	if (result >= 0)
	{
		result = ServerMain(result != 0);
	}
	ExitLog();
	return result;
}