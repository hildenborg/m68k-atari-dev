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

/*
	Option handling for this server is made to follow the real gdbserver documentation.
	However, most of the options for the real gdbserver is not applicable for us.

	gdbsrv.ttp	[options] [comm] prog [args]

	comm	(Default if missing: AUX)
	prog	(Default if missing: turns on option --multi)
		The executable you want to debug.
	args
		Commandline arguments for prog.
	options:
		--debug
			Outputs status information to console.
		--remote-debug
			Outputs remote package communication to console.
		--multi
			Starts the server without any executable to debug.
			Waits for gdb to connect with target extended-remote and tell us what executable to debug.
*/
int HandleOptions(int argc, char** argv)
{
	int result = 0;
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
			else if (StringCompare("COM", argv[i]) > 0)
			{
				StrCopy(argv[i], com_method);
				DbgOut("Using connection: ");
				DbgOut(argv[i]);
				DbgOut(newline);
			}
			else
			{
				StrCopy(argv[i], inferior_filename);
				//inferior_args = StrCopy(argv[i], inferior_args);
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