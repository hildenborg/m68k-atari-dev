/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

/*
	To add/change support for hardware memory registers, look at:
		
		context.c:
		StoreMemoryRegisters
		RestoreMemoryRegisters
		InferiorContextMemoryAddress
*/

// Don't use when serial communication, it's not good enough.
//#define QStartNoAckMode

#define qXfer_features
#include <stddef.h>
#include <stdbool.h>
#include "gdb_defines.h"
#include "bios_calls.h"
#include "server.h"
#include "exceptions.h"
#include "context.h"
#include "file_io.h"
#include "clib.h"
#include "comm.h"
#include "log.h"
#include "hex.h"
#include "packet.h"
#include "cookies.h"
#include "inferior.h"

typedef enum
{
	LISTEN_TO_GDB,
	CONTINUE_EXECUTION,
	KILL,
	RUN,
	RESTART
} LoopState;

#define USERCODE_SILENT		0
#define USERCODE_REPORT		1
#define USERCODE_ERROR		-1
#define USERCODE_WARNING	-2

const char serverFeatures[] = "PacketSize=3ff;swbreak+"
	#ifdef QStartNoAckMode
	";QStartNoAckMode+"
	#endif
	#ifdef qXfer_features
	";qXfer:features:read+"
	#endif
	;

/*
	Global states.
*/
bool	extendedMode = false;									// gdb extended-remote option
bool	loadInferiorRequested = false;							// Set when an action wants to load an inferior.
char	com_method[MAX_PATH_LEN] __attribute__((aligned(2)));	// Communication method. Only supports AUX for now.
bool	option_multi = false;
bool	run_once = false;										// If set and if extended mode, then gdbserver exits when inferior is killed.
int		userCodeForCommandLoop = USERCODE_SILENT;				// Used as si_code when calling ServerCommandLoop.
int		userCodeIfError = USERCODE_ERROR;						// Will be copied to userCodeForCommandLoop if inferior loading fails.

unsigned int	numOfCpuRegisters	=	18;

void OutputCrashInfo(void)
{
	ExceptionRegisters* registers = GetRegisters();
	ConOut("Crash report:\r\n");
	ConOutVal("D0", registers->d0);
	ConOutVal("D1", registers->d1);
	ConOutVal("D2", registers->d2);
	ConOutVal("D3", registers->d3);
	ConOutVal("D4", registers->d4);
	ConOutVal("D5", registers->d5);
	ConOutVal("D6", registers->d6);
	ConOutVal("D7", registers->d7);
	ConOutVal("A0", registers->a0);
	ConOutVal("A1", registers->a1);
	ConOutVal("A2", registers->a2);
	ConOutVal("A3", registers->a3);
	ConOutVal("A4", registers->a4);
	ConOutVal("A5", registers->a5);
	ConOutVal("A6", registers->a6);
	ConOutVal("SP", registers->sp);
	ConOutVal("SR", registers->sr);
	ConOutVal("PC", registers->pc);
	struct BasePage* serverBasePage = _BasePage; // Defined in crt0
	ConOutVal("Server _start: ", (unsigned int)serverBasePage->p_tbase);
}

int CheckServerQuitKey(void)
{
	if (Bconstat(DEV_CON) != 0)
	{
		return (((Bconin(DEV_CON) >> 16) & 0xff) == 62) ? -1 : 0;
	}
	return 0;
}

void SendStopCode(int si_signo, int si_code)
{
	bool start_break = IsInferiorStartBreak(si_signo, si_code);
	if (start_break)
	{
		ClearInferiorStartBreak();
	}
	WriteStop(si_signo, si_code, start_break);
}

void CmdSetWorkingDir(short vNameEnd)
{
	char buf[130];
	char *ptr = buf;
	char *endptr = buf;
	GetHexString(vNameEnd, &endptr);
	unsigned int len = endptr - ptr;
	*endptr = 0;
	if (len == 0)
	{
		// Reset to inferiors original working dir.
		ptr = inferior_workpath;
		len = strlen(ptr);
	}
	if (len >= 2 && ptr[1] == ':')
	{
		// Set drive
		unsigned short drive = ((unsigned char*)ptr)[0];
		drive -= drive >= 'a' ? 'a' : 'A';
		Dsetdrv(drive);
		ptr += 2;
		len -= 2;
	}
	if (len > 0)
	{
		// Set path, but make sure we make all / to \ first.
		for (char *p = ptr; *p != 0; ++p)
		{
			if (*p == '/') {*p = '\\';}
		}
		Dsetpath(ptr);
	}
}

void CmdQuery(void)
{
	short vNameEnd;
	char* inptr = GetInpacketPtr(0);
	if (StringCompare("qOffsets", inptr) > 0)
	{
		WriteOffsets();
	}
	else if (StringCompare("qSupported", inptr) > 0)
	{
		// We don't care about what the gdb client supports, we just report back what we support.
		WriteString(serverFeatures);
	}
	else if (StringCompare("QStartNoAckMode", inptr) > 0)
	{
		noAckMode = true;
		WriteOK();
	}
	else if ((vNameEnd = StringCompare("QSetWorkingDir:", inptr)) > 0)
	{
		CmdSetWorkingDir(vNameEnd);
	}
	else if ((vNameEnd = StringCompare("qXfer:features:read:target.xml:", inptr)) > 0)
	{
		WriteTargetXML(vNameEnd);
	}
	else if ((vNameEnd = StringCompare("qXfer:features:read:", inptr)) > 0)
	{
		WriteError(0);
	}
}

// Only support software breakpoints
void CmdSetBreakpoint(void)
{
	unsigned char* addr;
	unsigned int len;
	short offset = GetAddressAndLength(3, false, &addr, &len);
	char *inptr = GetInpacketPtr(1);
	if (offset > 0 && *inptr == '0')
	{
		if (InsertMemoryBreakpoint((unsigned short*)addr) == 0)
		{
			WriteOK();
		}
		else
		{
			WriteError(1);
		}
	}
}

// Only support software breakpoints
void CmdClearBreakpoint(void)
{
	unsigned char* addr;
	unsigned int len;
	short offset = GetAddressAndLength(3, false, &addr, &len);
	char *inptr = GetInpacketPtr(1);
	if (offset > 0 && *inptr == '0')
	{
		RemoveMemoryBreakpoint((unsigned short*)addr);
		WriteOK();
	}
}

LoopState CmdContinue(bool trace)
{
	if (inferiorState == NOT_LOADED)
	{
		// Can't continue not existing inferior...
		// Documentation doesn't specify what to do if continue cannot be performed.
		// However, gdb treats an empty answer as command not supported, so we'll go for that. 
		return LISTEN_TO_GDB;
	}
	
	ExceptionRegisters* er = GetRegisters();
	unsigned int add;
	if (ReadNumber(1, &add) > 0)
	{
		// Set a new address to continue/step at.
		er->pc = add;
	}
	if (trace)
	{
		er->sr |= 0x8000;
	}
	else
	{
		er->sr &= ~0x8000;
	}
	return CONTINUE_EXECUTION;
}

bool CheckFileCmdArgs(const char* cmd, short args, const char* argv0, short argc)
{
	return (StringCompare(cmd, argv0) > 0 && args == argc);
}

LoopState CmdFileOperation(short cmdEnd)
{
	int ioErrno = VFILE_ERRNO_EINVAL;
	int result = -1;
	char *attachment = 0;

	// Find operand and arguments and end all with 0
	char* argv[4];
	short argc = GetFileArgs(cmdEnd, argv);

	char* inptr = GetInpacketPtr(0);

	if (CheckFileCmdArgs("open", 4, argv[0], argc))
	{
		HexConvertByteArray(argv[1]);
		char *filename = argv[1];
		VfileFixPath(filename);
		int flags = HexToVariable(argv[2]);
		//int mode = HexToVariable(argv[3]);	// Not supported on TOS
		result = VfileOpen(filename, flags, &ioErrno);
	}
	else if (CheckFileCmdArgs("close", 2, argv[0], argc))
	{
		int fd = HexToVariable(argv[1]);
		result = VfileClose(fd, &ioErrno);
	}
	else if (CheckFileCmdArgs("pread", 4, argv[0], argc))
	{
		int fd = HexToVariable(argv[1]);
		int count = HexToVariable(argv[2]);
		int offset = HexToVariable(argv[3]);
		/*
			We have decoded everything in the inPacket (inptr), so we can use it as a read buffer.
			We need to estimate how many hex encoded bytes we can read.
		*/
		int maxRead = (PACKET_SIZE - 20);	// max packet size - some room for response
		if (count > maxRead) {count = maxRead;}
		result = VfileRead(fd, inptr, offset, count, &ioErrno);
		if (result >= 0)
		{
			attachment = inptr;
		}
	}
	else if (CheckFileCmdArgs("pwrite", 4, argv[0], argc))
	{
		int fd = HexToVariable(argv[1]);
		int offset = HexToVariable(argv[2]);
		char *data = argv[3];
		int count = (int)((GetInpacketPtr(GetInPacketLength())) - data);
		result = VfileWrite(fd, data, offset, count, &ioErrno);
	}
	else if (CheckFileCmdArgs("fstat", 2, argv[0], argc))
	{
		int fd = HexToVariable(argv[1]);
		vfile_stat* stat = (vfile_stat*)inptr;	// Use inPacket (inptr) as a buffer.
		result = VfileFstat(fd, stat, &ioErrno);
		if (result >= 0)
		{
			attachment = (char*)stat;
		}
	}
	else if (CheckFileCmdArgs("stat", 2, argv[0], argc))
	{
		HexConvertByteArray(argv[1]);
		char *filename = argv[1];
		VfileFixPath(filename);
		vfile_stat* stat = (vfile_stat*)inptr;	// Use inPacket (inptr) as a buffer.
		result = VfileStat(filename, stat, &ioErrno);
		if (result >= 0)
		{
			attachment = (char*)stat;
		}
	}
	else if (CheckFileCmdArgs("unlink", 2, argv[0], argc))
	{
		HexConvertByteArray(argv[1]);
		char *filename = argv[1];
		VfileFixPath(filename);
		result = VfileDelete(filename, &ioErrno);
	}
	else if (CheckFileCmdArgs("setfs", 2, argv[0], argc))
	{
		// unsigned int pid = HexToVariable(argv[1]);
		// Don't need to do anything. File system is the same for all processes in TOS.
		result = 0;
	}
	WriteFileResponse(result, ioErrno, attachment);
	return LISTEN_TO_GDB;
}

LoopState CmdFlexible(void)
{
	short vNameEnd;
	char* inptr = GetInpacketPtr(0);

	if ((vNameEnd = StringCompare("vRun;",  inptr)) > 0)
	{
		if (inferiorState != NOT_LOADED)
		{
			// Return error as we don't support multiple running executables.
			WriteError(1);
			return LISTEN_TO_GDB;
		}

		// Get filename and command line arguments
		// If filename is empty, then restart previous inferior.
		GetInferiorNameAndArgs(vNameEnd, inferior_filename, inferior_cmdline);
		DbgOut("Run: \r\n\tinferior: ");
		DbgOut(inferior_filename);
		DbgOut("\r\n\targs: ");
		DbgOut(&inferior_cmdline[1]);
		DbgOut(newline);
		return RUN;
	}
	else if ((vNameEnd = StringCompare("vKill;",  inptr)) > 0)
	{
		return KILL;
	}
	else if ((vNameEnd = StringCompare("vFile:",  inptr)) > 0)
	{
		return CmdFileOperation(vNameEnd);
	}
	return LISTEN_TO_GDB;
}

LoopState CmdReportStopReason(int si_signo, int si_code, bool* skipAck)
{
	LoopState loopState = LISTEN_TO_GDB;
	if (inferiorState == NOT_LOADED)
	{
		if (option_multi && inferior_filename[0] != 0)
		{
			// Need to load and start inferior
			userCodeIfError = USERCODE_WARNING;
			loopState = RUN;
		}
		else
		{
			// gdbserver returns W00, and so do we.
			WriteString("W00");	// Means that pcrocess 00 have exited.
			if (!extendedMode)
			{
				// GDB expects us to exit gdbserver now.
				loopState = KILL;
				// Send packet also...
				TransmitPacket(*skipAck);
			}
			*skipAck = true; // Even for extended mode?				
		}
	}
	else if (inferiorState == RUNNING)
	{
		// We have indeed a running inferior and it have cast an exception.
		SendStopCode(si_signo, si_code);
	}
	else if (inferiorState == LOADED)
	{
		// Need to start the inferior to deliver the resonse.
		loopState = CONTINUE_EXECUTION;
	}
	else
	{
		// Don't even know how to get here... Well, return OK anyway just to make the code look good.
		WriteOK();
	}
	return loopState;
}

LoopState HandleBreakResponse(int si_signo, int si_code, bool* isSupervisorMode)
{
	LoopState loopState = LISTEN_TO_GDB;
	if (si_signo == GDB_SIGUSR1)
	{
		/*
			We get here only if ServerCommandLoop have been called from ServerMain.
			This means that we are in user mode and no inferior is being executed.
			There may still be an inferior loaded into memory.
		*/
		*isSupervisorMode = false;		
		if (si_code == USERCODE_ERROR)
		{
			WriteError(1);
			TransmitPacket(false);
		}
		else if (si_code == USERCODE_WARNING)
		{
			WriteString("W00");
			TransmitPacket(false);
		}
		else if (si_code == USERCODE_REPORT)
		{
			// We end up here if we are in extended mode and have loaded an inferior using the vRun command.
			// Need to start the inferior to deliver the resonse.
			loopState = CONTINUE_EXECUTION;
			userCodeForCommandLoop = USERCODE_SILENT;
		}
	}
	else
	{
		/*
			We get here if any of the following have happened:
				An exception have occured.
				A breakpoint have been reached.
				CTRL-C have been pressed.
			All of those cases means that we have come here from an interrupt or exception.
			So we know for a fact that we are in supervisor mode.
			This also means that we must have an inferior being executed (but paused now).
		*/
		*isSupervisorMode = true;		
		if (IsServerException())
		{
			// Exception in gdbserver code!
			ConOut("\tError - Exception in gdbserver code! ABORTING!\r\n");
			ConOutVal("si_signo", (unsigned int)si_signo);
			ConOutVal("si_code", (unsigned int)si_code);
			OutputCrashInfo();
			si_signo = GDB_SIGABRT;
			loopState = KILL;
		}
		// Write stop packet to gdb.
		SendStopCode(si_signo, si_code);
		TransmitPacket(false);
	}
	return loopState;
}

void HandleCommandLoopExit(LoopState loopState, int si_signo)
{
	if (loopState == KILL)
	{
		// If we have an inferior, we need to terminate it properly.
		DbgOut("ServerCommandLoop: command KILL.\r\n");
		DbgOutVal("\tinferiorState: ", inferiorState);
		if (inferiorState == LOADED)
		{
			// Not started, so we just need to free the memory taken by it.
			Mfree((struct BasePage*)inferiorBasePage);
			inferiorBasePage = NULL;
			inferiorState = NOT_LOADED;
		}
		else if (inferiorState == RUNNING)
		{
			TerminateInferior(si_signo);	// no-return function! Code execution continues after RunInferior below.
		}
		else if (inferiorState == NOT_LOADED)
		{
			// If we don't have any inferior loaded and still requests to kill,
			// then let's assume that we want to shut down the gdbserver.
			// Meaning: end of extendedMode if set.
			extendedMode = false;
		}
	}
	else if(loopState == CONTINUE_EXECUTION)
	{
		/*
			We can only get here with inferiorState as LOADED or RUNNING.
			CmdContinue will handle inferiorState NOT_LOADED.
		*/
		comDev->EnableCtrlC(true);	// So we can break with Ctrl-C
		if (inferiorState == LOADED)
		{
			int return_code = 0;
			if (!RunInferior(&return_code))	// Returns after inferior is terminated.
			{
				// Inferior terminated itself. 
				// Report process exit and inferior return code to gdb.
				ClearOutPacket();
				WriteChar('W');
				WriteByte((unsigned char)return_code);
				TransmitPacket(true);	// We don't expect any answer.
			}
			else
			{
				// Terminated by server
				if (return_code == -1)
				{
					// Bad error, need to exit the extended remote protocol if set, so we will exit the main gdbserver loop.
					extendedMode = false;
				}
			}
		}
		else
		{
			DbgOut("ServerCommandLoop: continuing...\r\n");
		}
	}
	else if(loopState == RUN)
	{
		if (inferiorState != NOT_LOADED)
		{
			// We don't currently support multiple inferiors running.
			// vRun command returns error for it.
			DbgOut("WARNING: Running inferior without unloading previous one.\r\n");
			DbgOut("This gdbserver do not support multiple threads or processes.\r\n");
		}
		loadInferiorRequested = true;
		userCodeForCommandLoop = USERCODE_REPORT;
	}
	else if(loopState == RESTART)
	{
		// Kill and Run inferior.
		if (inferiorState == RUNNING)
		{
			loadInferiorRequested = true;
			userCodeForCommandLoop = USERCODE_REPORT;
			TerminateInferior(si_signo);	// no-return function! Code execution continues in RunInferior.
		}
	}
}

void ServerCommandLoop(int si_signo, int si_code)
{
	/*
		CTRL-C should have been disabled already by the comm extension, but do it again just to be safe.
	*/
	comDev->EnableCtrlC(false);

	DbgOut("ServerCommandLoop: \r\n");
	DbgOutVal("si_signo", (unsigned int)si_signo);
	DbgOutVal("si_code", (unsigned int)si_code);

	ClearOutPacket();
	bool isSupervisorMode = false;
	LoopState loopState = HandleBreakResponse(si_signo, si_code, &isSupervisorMode);

	// Command loop, get packet from gdb and transmit response.
	while (loopState == LISTEN_TO_GDB)
	{
		bool skipAck = false;
		ClearOutPacket();	// Keep track of the outPacket count and write pos
		ReceivePacket();
		char* inptr = GetInpacketPtr(0);
		switch (inptr[0])
		{
		case 0x03:	// Ctrl-C. 
			// The user have requested to pause execution of inferior, but we are already paused or not even running...
			if (inferiorState == RUNNING)
			{
				// Just repeat the last stop code.
				SendStopCode(si_signo, si_code);
			}
			else
			{
				// Undefined area... Just return OK.
				WriteOK();
			}
			break;
		case 0x1a:	// Ctrl-Z. 
			// Shut down the server and exit.
			run_once = true;
			option_multi = false;
			loadInferiorRequested = false;
			loopState = KILL;
			break;
		case '!':	// Asks if we are a target-remote-extended server. Returning OK places us in extended mode.
			extendedMode = true;
			WriteOK();
			break;
		case 'D':	// Detach. Most gdb MI clients seem to expect us to kill the inferior here.
			//WriteOK();
			loopState = KILL;
			break;
		case 'H':	// Set thread number. Just return OK.
			/*
				gdbserver returns E01
			*/
			WriteOK();
			break;
		case 'T':	// Set active thread. Just return OK.
			WriteOK();
			break;
		case 'R':	// Restart inferior.
			loopState = RUN;
			break;
		case 'v':	// Flexibillity packets. Like file handling, continuing, running...
			loopState = CmdFlexible();
			break;
		case '?':	// Report the exception
			loopState = CmdReportStopReason(si_signo, si_code, &skipAck);
			break;
		case 'g':	// Get register values
			ReadRegisters();
			break;
		case 'G':	// Set register values
			WriteRegisters();
			break;
		case 'm':	// Read from memory
			ReadMemory(isSupervisorMode);
			break;
		case 'M':	// Write to memory
			WriteMemory(isSupervisorMode);
			break;
		case 'p':	// Get specific register
			ReadRegister();
			break;
		case 'P':	// Set specific register
			WriteRegister();
			break;
		case 's':	// Step
			loopState = CmdContinue(true); 
			break;
		case 'c':	// Continue
			loopState = CmdContinue(false); 
			break;
		case 'k':	// Kill
			loopState = KILL;
			break;
		case 'q':	// Query
			CmdQuery();
			break;
		case 'Q':	// Query set
			CmdQuery();
			break;
		case 'z':	// Clear breakpoint
			CmdClearBreakpoint();
			break;
		case 'Z':	// Set breakpoint
			CmdSetBreakpoint();
			break;
		default:
			DbgRemOut("\tNot supported, ignoring...\r\n");
			break;
		}
		if (loopState == LISTEN_TO_GDB)
		{
			TransmitPacket(skipAck);
		}
	}
	
	HandleCommandLoopExit(loopState, si_signo);

	DbgOut("ServerCommandLoop: exiting\r\n");
}

int ServerMain(bool loadRequest)
{
	DbgOut("ServerMain: Server initing.\r\n");
	loadInferiorRequested = loadRequest;
	int ret = -1;
	// Get cookies
	Supexec(GetCookies);
	if ((Cookie_FPU & (0x1f << 16)) != 0 && Cookie_CPU >= 20)
	{
		numOfCpuRegisters = 29;
	}
	comDev = InitComm(com_method); 
	if (comDev == 0)
	{
		ConOut("Failed creating communication method.");
		return -1;
	}
	comDev->EnableCtrlC(false); // Init as off

	if (CreateServerContext() < 0)
	{
		ConOut("Could not create server context.");
		return -1;
	}
	DbgOut(comDev->DeviceName());
	
	InitFileIO();

	inferiorState = NOT_LOADED;
	
	DbgOut("ServerMain: Server started.\r\n");

	// If we get extended remote protocol, then we just keep looping.
	// This allows for restarting the inferior or starting another.
	extendedMode = false;
	userCodeForCommandLoop = USERCODE_SILENT;
	do
	{
		if (loadInferiorRequested)
		{
			// Load inferior
			/*
				If inferior is an empty file, strange things happen...
				Need some better fail safe here.
			*/
			if (LoadInferior(inferior_filename, inferior_cmdline, NULL) < 0)
			{
				DbgOut("Could not load inferior: ");
				DbgOut(inferior_filename);
				DbgOut(newline);
				
				if (userCodeForCommandLoop == USERCODE_REPORT)
				{
					userCodeForCommandLoop = userCodeIfError;
				}
				else
				{
					// Exit gdbserver if *command line* specified inferior could not be loaded.
					break;
				}
			}
			userCodeIfError = USERCODE_ERROR;
			loadInferiorRequested = false;
		}
		ServerCommandLoop(GDB_SIGUSR1, userCodeForCommandLoop);
		DiscardAllBreakpoints();
		ret = 0;
	} while ((extendedMode && !run_once) || option_multi);

	ExitFileIO();
	
	DestroyServerContext();

	if (log_debug || log_debug_remote || ret < 0)
	{
		ConOut("Press any key...");
		// Wait for keypress
		while (Bconstat(DEV_CON) == 0)
		{
		}
	}
	return ret;
}

