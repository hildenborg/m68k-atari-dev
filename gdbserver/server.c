/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

/*
	For future feature additions:
	
	To add support for systems with fpu or additional registers, then the following code needs some attention:
	
		exceptions.h:
		struct ExceptionRegisters
		
		critical.s:
		SaveState:
		RestoreState:
	
		server.c:
		numOfCpuRegisters
		WriteStop
		ReadRegisters
		WriteRegisters
		ReadRegister
		WriteRegister

	To add support for more exceptions (like for fpu), then look at:

		exceptions.c:
		Exception

		critical.s:
		.macro HookVector
		.macro UnHookVector
		.macro Hook
		.macro Hijack
		InitExceptions
		RestoreExceptions

	To add support for other serial communications than the original ST, look at:
	
		server.c:
		ServerMain
		GetByte
		PutByte
	
		critical.s:
		S_Hook
		DCD_Hook
		
	To add/change support for hardware memory registers, look at:
		
		context.c:
		StoreMemoryRegisters
		RestoreMemoryRegisters
		InferiorContextMemoryAddress
		
*/

// The maximum path length for the system.
// Legacy ST is 128, but we might need a zero at the end too.
#define MAX_PATH_LEN	130

// Disable all exceptions. Useful when debugging packet handling.
//#define DISABLE_EXCEPTIONS

// Set --debug and --remote-debug to active.
//#define DEBUG_OPTIONS_ON

// Don't use when serial communication, it's not good enough.
//#define QStartNoAckMode

#define qXfer_features
#include <stddef.h>
#include <stdbool.h>
#include "gdb_signals.h"
#include "bios_calls.h"
#include "server.h"
#include "exceptions.h"
#include "context.h"
#include "critical.h"
#include "target_xml.h"

typedef enum
{
	LISTEN_TO_GDB,
	CONTINUE_EXECUTION,
	KILL,
	RUN,
	RESTART
} LoopState;

typedef enum
{
	NOT_LOADED,
	LOADED,
	RUNNING
} InferiorState;

#define USERCODE_SILENT	0
#define USERCODE_REPORT	1
#define USERCODE_ERROR	-1

#define PACKET_SIZE 0x3ff
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
volatile struct BasePage*		inferiorBasePage = NULL;		// basepage for debugged exectable
volatile InferiorState	inferiorState = NOT_LOADED;		// To know if we have an inferior and if we have started it or not.
volatile bool			extendedMode = false;			// gdb extended-remote option
volatile bool			loadInferiorRequested = false;	// Set when an action wants to load an inferior.

char	inferior_filename[MAX_PATH_LEN] __attribute__((aligned(2)));	// The filename of the inferior being debugged. Can be empty if nothing is loaded.
char	inferior_cmdline[MAX_PATH_LEN] __attribute__((aligned(2)));		// Command line args to debugged inferior.
char	inferior_workpath[MAX_PATH_LEN] __attribute__((aligned(2)));	// The work path of the inferior being debugged. Can be empty if nothing is loaded.
char	com_method[MAX_PATH_LEN] __attribute__((aligned(2)));			// Communication method. Only supports COM1 for now.

bool			option_multi = false;
bool			run_once = false;				// If set and if extended mode, then gdbserver exits when inferior is killed.
bool			log_debug = false;				// cmdline option --debug
bool			log_debug_remote = false;		// cmdline option --debug-remote
bool			noAckMode = false;				// gdb QStartNoAckMode
unsigned short* __start_Breakpoint = NULL;		// Only set during startup of inferior, and used to break at __start.
int				userCodeForCommandLoop = USERCODE_SILENT;	// Used as si_code when calling ServerCommandLoop.

unsigned int	numOfCpuRegisters	=	18;

/* 
	_CPU: 
	0 = 68000
	10 = 68010
	20 = 68020
	30 = 68030
	40 = 68040
	60 = 68060
*/
unsigned int Cookie_CPU = 0;
/*
	_VDO (shifter [high word, low word]):
	[0,0] = st
	[1,0] = ste
	[2,0] = tt
	[3,0] = falcon
*/
unsigned int Cookie_VDO = 0;
/*
	_FPU (float unit):
	High word bit 0 = SFP004
	High word bits 1 - 2 =
		01	= 68881 or 68882
		10	= 68881
		11	= 68882
	High word bit 3 = 68040 Internal
	High word bit 4 = 68060 Internal

	Note:
	If any of the bits 1-4 is set, then we know that we have a system that can run fpu instructions directly.
*/
unsigned int Cookie_FPU = 0;
/*
	_MCH (machine [high word, low word]):
	[0,0] = st
	[1,0] = ste
	[1,8] = st book
	[1,16] mega ste
	[2,0] = tt
	[3,0] = falcon
*/
unsigned int Cookie_MCH = 0;

/*
	Max inPacket and outPacket size must be the same, 
	and must also be set as size-1 in serverFeatures.
*/
short inPacketLength = 0;
char	inPacket[PACKET_SIZE + 1] __attribute__((aligned(2)));
short outPacketLength = 0;
char	outPacket[PACKET_SIZE + 1] __attribute__((aligned(2)));

const char hex[] = "0123456789abcdef";

const char newline[] = "\r\n";

// We need to provide this as we don't use the standard libraries, and GCC optimization will
// Convert test loops into strlen calls...
size_t strlen(const char* s)
{
	size_t len = 0;
	__asm__ volatile (
		"move.l	%1, %0\n\t"
		"1:\n\t"
		"tst.b	%1@+\n\t"
		"bne.s	1b\n\t"
		"sub.l	%1, %0\n\t"		// -(len + 1)
		"not.l	%0\n\t"			// !(-(len + 1)) = len
		: "=d" (len)
		: "a" (s)
		:);
	return len;

//	size_t len = 0;
//	while (s[len] != 0) {++len;}
//	return len;
}

#define NibbleToHex(nibb) hex[(nibb) & 0xf]

void ConOut(const char* txt)
{
	int len = 0;
	while (txt[len] != 0) 
	{
		Bconout(DEV_CONSOLE, txt[len]);
		++len;
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

void DbgOutVal(const char* name, unsigned int val)
{
	char buf[12];
	int i = 8;
	buf[i] = 0;
	while (--i >= 0)
	{
		buf[i] = NibbleToHex(val);
		val = val >> 4;
	}
	DbgOut("\t");
	DbgOut(name);
	DbgOut(": 0x");
	DbgOut(buf);
	DbgOut(newline);
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

int LoadInferior(const char* fileName, const char* cmdLine, const char* environment)
{
	DbgOut("LoadInferior: ");
	int loadres = -1;
	// Check that we doesn't already have an inferior loaded.
	if (inferiorState == NOT_LOADED)
	{
		// Load inferior
		loadres = Pexec(PE_LOAD, fileName, cmdLine, environment);
		if (loadres > 0)
		{
			inferiorBasePage = (volatile struct BasePage*)loadres;
			// Set one time breakpoint at _start.
			__start_Breakpoint = (unsigned short*)(inferiorBasePage->p_tbase);
			if (InsertMemoryBreakpoint(__start_Breakpoint) != 0)
			{
				__start_Breakpoint = NULL;
			}
			inferiorState = LOADED;
			DbgOut("LOADED\r\n");
		}
		else
		{
			inferiorBasePage = NULL;
			inferiorState = NOT_LOADED;
			DbgOut("NOT_LOADED\r\n");
		}
	}
	else
	{
			DbgOut("Error - not unloaded!\r\n");
	}
	return loadres;
}

bool inferiorTerminatedByServer = false;

bool RunInferior(int* return_code)
{
	DbgOut("RunInferior: Started\r\n");
	// Run inferior while saving and restoring various system properties.
	inferiorState = RUNNING;
	inferiorTerminatedByServer = false;
	*return_code = Pexec(PE_GOTHENFREE, 0, (const char*)inferiorBasePage, 0);
	// Returning from a terminated inferior
	inferiorState = NOT_LOADED;
	inferiorBasePage = NULL;	// inferior have been unloaded.
	if (inferiorTerminatedByServer)
	{
		if (*return_code == -1)
		{
			// Bad error, need to exit the extended remote protocol if set, so we will exit the main gdbserver loop.
			extendedMode = false;
		}
	}
	DbgOut("RunInferior: Terminated\r\n");
	return inferiorTerminatedByServer;
}

void __attribute__ ((noreturn)) TerminateInferior(int si_signo)
{
	DbgOut("TerminateInferior: Terminating...\r\n");
	inferiorTerminatedByServer = true;
	Pterm(si_signo == GDB_SIGABRT ? -1 : -32);
	// The code execution will not continue here. It will continue after Pexec() in RunInferior().
}

char* StrCopy(const char* source, char* dest)
{
	char c;
	while ((c = *source++) != 0)
	{
		*dest++ = c;
	}
	*dest = 0;
	return dest;
}

int HexToNibble(char c)
{
	return (c - (c < 'A' ? '0' : ((c < 'a' ? 'A' : 'a') - 10))) & 0xf;
}

unsigned char HexToByte(char* ptr)
{
	return (unsigned char)((HexToNibble(ptr[0]) << 4) | HexToNibble(ptr[1]));
}

// Endian aware
unsigned int HexToLong(char* ptr)
{
	unsigned int val = 0;
	for (int i = 0; i < 4; ++i)
	{
		val = (val << 8 ) | HexToByte(ptr);
		ptr += 2;
	}
	return val;
}

short GetHexString(short offset, char** strOut)
{
	char* str = *strOut;
	short i = offset;
	while (i < inPacketLength && inPacket[i] != ';')
	{
		*str++ = (char)HexToByte(inPacket + i);
		i += 2;
	}
	if (inPacket[i] == ';')
	{
		++i;
	}
	*strOut = str;
	return i;
}

int CheckServerQuitKey(void)
{
	if (Bconstat(DEV_CON) != 0)
	{
		return (((Bconin(DEV_CON) >> 16) & 0xff) == 62) ? -1 : 0;
	}
	return 0;
	// Check if Shift-F4 have been released
//	volatile unsigned char* keyboardACIA = (unsigned char*)0xfffffc02;
//	return *keyboardACIA == (62|0x80) ? -1 : 0;
}

int GetByte(void)
{
	while (Bconstat(DEV_AUX) == 0)
	{
		if (!GetDCD())
		{
			// Lost contact
			return -1;
		}
		if (CheckServerQuitKey() < 0)
		{
			return 0x1a;
		}
	}
	return Bconin(DEV_AUX) & 0xff;
}

void PutByte(char ch)
{
	while (Bcostat(DEV_AUX) == 0)
	{
		if (!GetDCD())
		{
			// Lost contact
			return;
		}
	}
	Bconout(DEV_AUX, (unsigned short)ch);
}

void ReceivePacket(void)
{
	DbgRemOut("ReceivePacket: \r\n");
	int c;
	unsigned char sum = 0;
	bool waitForPacket = true;

	while (waitForPacket)
	{
		inPacketLength = 0;
		DbgRemOut("\tWaiting...\r\n");
		
		// Wait for connection.
		while (!GetDCD())
		{
			// Check keypress
			// If we do not have any connection yet, then we cannot have any running inferiors, which means that we cannot be in supervisor mode.
			// Meaning, to peek the keycode, we need to enter supervisor mode.
			if (CheckServerQuitKey() < 0)
			{
				DbgOut("\r\n\tKill Server!\r\n");
				inPacketLength = 1;
				inPacket[0] = 0x1a;		// Ctrl-Z
				return;
			}
		}

		// Wait for packet start
		while ((c = GetByte()) != '$')
		{
			if (c < 0)
			{
				DbgRemOut("\r\n\tConnection dropped!\r\n");
				c = 'k';
				break;
			}
			else if (c == 0x03)
			{
				DbgRemOut("\r\n\tCtrl-C!\r\n");
				break;
			}
			else if (c == 0x1a)
			{
				DbgOut("\r\n\tKill Server!\r\n");
				break;
			}
		}
		if (c != '$')
		{
			// Either got a dropped connection or a Ctrl-C
			inPacketLength = 1;
			inPacket[0] = (unsigned char)c;
			return;
		}
		
		DbgRemOut("\tGot beginning of packet.\r\n");
		// Fetch payload
		bool escaped = false;
		while ((c = GetByte()) != '#') 
		{
			if (c < 0)
			{
				DbgRemOut("\r\n\tConnection dropped!\r\n");
				inPacketLength = 1;
				inPacket[0] = 'k';	// Kill inferior
				return;
			}
			else if (c == '$')
			{
				inPacketLength = 0;
				sum = 0;
				escaped = false;
			}
			sum += (unsigned char)c;
			if (c == 0x7d)
			{
				escaped = true;
			}
			else
			{
				if (escaped) { c ^= 0x20; }
				inPacket[inPacketLength++] = (unsigned char)c;
				escaped = false;
			}
		}
		inPacket[inPacketLength] = 0;
		DbgRemOut("\tGot end of packet.\r\n\t");
		DbgRemOut(inPacket);
		// Fetch checksum
		char csum[2];
		csum[0] = (char)GetByte();
		csum[1] = (char)GetByte();
		if (!noAckMode)
		{
			unsigned char psum = (unsigned char)((HexToNibble(csum[0]) << 4) | HexToNibble(csum[1]));
			waitForPacket = sum != psum;
			PutByte(waitForPacket ? '-' : '+');
			if (!waitForPacket)
			{
				DbgRemOut("\r\n\tPacket checksum OK.\r\n");
			}
			else
			{
				DbgRemOut("\r\n\tError - Packet checksum not OK!\r\n");
			}
		}
		else
		{
			waitForPacket = false;
		}
		if (!GetDCD())
		{
			DbgRemOut("\r\n\tConnection dropped!\r\n");
			inPacketLength = 1;
			inPacket[0] = 'k';	// Kill inferior
			return;
		}
	}
}

void TransmitPacket(bool skipAck)
{
	DbgRemOut("TransmitPacket:\r\n\t");
	outPacket[outPacketLength] = 0;
	DbgRemOut(outPacket);

	if (!GetDCD())
	{
		// Connection dropped...
		DbgRemOut("\r\n\tConnection dropped!\r\n");
		return;
	}
	
	unsigned char sum = 0;
	// Send encoded
	PutByte('$');	// Packets always start with $
	for (int i = 0; i < outPacketLength; ++i)
	{
		char c = outPacket[i];
		if (c == '$' || c == '#' || c == 0x7d)
		{
			PutByte(0x7d);
			sum += 0x7d;
			c ^= 0x20;
		}
		PutByte(c);
		sum += (unsigned char)c;
	}
	// End with checksum
	PutByte('#');
	PutByte(NibbleToHex(sum >> 4));
	PutByte(NibbleToHex(sum));

	if (!noAckMode && !skipAck)
	{
		// Wait for ack
		int ack = GetByte();
		if (ack == '+')
		{
			DbgRemOut("\r\n\tAck OK.\r\n");
		}
		else if (ack < 0)
		{
			// Connection dropped...
			DbgRemOut("\r\n\tConnection dropped!\r\n");
		}
		else
		{
			DbgRemOut("\r\n\tError - Ack not OK!\r\n");
		}
	}
	else
	{
		DbgRemOut("\r\n");
	}
}

#define WriteChar(c) outPacket[outPacketLength++] = c

void WriteByte(unsigned char c)
{
	WriteChar(NibbleToHex(c >> 4));
	WriteChar(NibbleToHex(c));
}

// Endian aware
void WriteLong(unsigned int val)
{
	for (int i = 28; i >= 0; i -= 4)
	{
		WriteChar(NibbleToHex(val >> i));
	}
}

void WriteString(const char* str)
{
	char c;
	while ((c = *str++) != 0)
	{
		WriteChar(c);
	}
}

void WriteNameAndLong(const char* name, unsigned int v)
{
	WriteString(name);
	WriteChar('=');
	WriteLong(v);
}

#define WriteSemiColon() WriteChar(';')
#define WriteOK() WriteString("OK")
#define WriteEQ() WriteString("EQ")
#define WriteNE() WriteString("NE")

void WriteError(int e)
{
	WriteChar('E');
	WriteByte((unsigned char)e);
}

void WriteStop(int si_signo, int si_code)
{
	if (si_signo == GDB_SIGTRAP && si_code == TRAP_BRKPT)
	{
		// Software breakpoint hit.
		WriteChar('T');
		WriteByte((unsigned char)si_signo);
		unsigned int* ptr = (unsigned int*)GetRegisters();
		if ((unsigned int)__start_Breakpoint == GetRegisters()->pc)
		{
			// We get here the first time we start the inferior.
			// Take the opportunity of capturing the inferiors work directory.
			/*
			unsigned short drive = Dgetdrv();
			if (Dgetpath(inferior_workpath + 3, drive) < 0 || strlen(inferior_workpath + 3) == 0)
			{
				inferior_workpath[3] = '.';
				inferior_workpath[4] = 0;
			}
			inferior_workpath[0] = 'A' + (char)drive;
			inferior_workpath[1] = ':';
			inferior_workpath[2] = '\\';
			DbgOut("Inferior work path: ");
			DbgOut(inferior_workpath);
			DbgOut(newline);
			*/
			if (RemoveMemoryBreakpoint(__start_Breakpoint) == 0)
			{
				__start_Breakpoint = NULL;
			}
		}
		else
		{
			WriteString("swbreak:;");
		}
		// Report fp, sp, sr, pc
		for (unsigned char i = 14; i <= 17; ++i)
		{
			WriteByte(i);
			WriteChar(':');
			WriteLong(ptr[i]);
			WriteChar(';');
		}
	}
	else
	{
		WriteChar('S');
		WriteByte((unsigned char)si_signo);
	}
}

void ReadRegisters()
{
	unsigned int* ptr = (unsigned int*)GetRegisters();
	size_t num = 18;
	if ((Cookie_FPU & (0x1f << 16)) != 0)
	{
		num += (8*3) + 3;
	}

	for (size_t i = 0; i < num; ++i)
	{
		WriteLong(ptr[i]);
	}
}

void WriteRegisters(void)
{
	if (inPacketLength != (sizeof(ExceptionRegisters) * 2) + 1)
	{
		WriteError(1);
	}
	else
	{
		unsigned int* ptr = (unsigned int*)GetRegisters();
		char* buf = inPacket + 1;
		size_t num = 18;
		if ((Cookie_FPU & (0x1f << 16)) != 0)
		{
			num += (8*3) + 3;
		}
		for (size_t i = 0; i < num; ++i)
		{
			ptr[i] = HexToLong(buf);
			buf += 8;
		}
		WriteOK();
	}
}

short ReadNumber(short inOff, unsigned int* result)
{
	unsigned int v = 0;
	short offset = inOff;

	while (offset < inPacketLength && inPacket[offset] != '=')
	{
		char c = inPacket[offset];
		if (c == '=' || c == ',' || c == ':') { break; }
		v = (v << 4) | (unsigned int)HexToNibble(c);
		++offset;
	}
	*result = v;

	return (inOff == offset) ? -1 : offset;
}

void WriteRegister(void)
{
	unsigned int* rptr = (unsigned int*)GetRegisters();
	short offset;
	unsigned int idx;
	if ((offset = ReadNumber(1, &idx)) > 0)
	{
		if (idx < numOfCpuRegisters)
		{
			if (inPacket[offset] == '=')
			{
				if (idx < 18)
				{
					rptr[idx] = HexToLong(inPacket + offset + 1);
				}
				else if (idx >= (18 + 8))
				{
					idx += 16;
					rptr[idx] = HexToLong(inPacket + offset + 1);
				}
				else
				{
					idx = 18 + ((idx - 18) * 3);
					rptr[idx] = HexToLong(inPacket + offset + 1);
					rptr[idx + 1] = HexToLong(inPacket + offset + 9);
					rptr[idx + 2] = HexToLong(inPacket + offset + 17);
				}
			}
		}
		else
		{
			// Just ignore.
		}
	}

	WriteOK();
}

void ReadRegister(void)
{
	unsigned int* rptr = (unsigned int*)GetRegisters();
	unsigned int idx;
	if (ReadNumber(1, &idx) > 0)
	{
		if (idx < numOfCpuRegisters)
		{
			if (idx < 18)
			{
				WriteLong(rptr[idx]);
			}
			else if (idx >= (18 + 8))
			{
				idx += 16;
				WriteLong(rptr[idx]);
			}
			else
			{
				idx = 18 + ((idx - 18) * 3);
				WriteLong(rptr[idx]);
				WriteLong(rptr[idx + 1]);
				WriteLong(rptr[idx + 2]);
			}
		}
		/*
		else
		{
			WriteLong(0x0);
		}
		*/
	}
}

short GetAddressAndLength(short offset, bool write, unsigned char** addr, unsigned int* len)
{
	unsigned int add;
	if ((offset = ReadNumber(offset, &add)) > 0)
	{
		*addr = (unsigned char*)add;
		if (inPacket[offset] == ',')
		{
			if ((offset = ReadNumber(offset + 1, len)) > 0)
			{
				if (write)
				{
					if (inPacket[offset] == ':')
					{
						++offset;
						if ((offset + (short)(*len * 2)) == inPacketLength)
						{
							return offset;
						}
					}
				}
				else
				{
					return offset;
				}
			}
		}
	}
	return -1;
}

void WriteMemory(void)
{
	unsigned char* addr;
	unsigned int len;
	short offset = GetAddressAndLength(1, true, &addr, &len);
	if (offset > 0)
	{
		char* ptr = inPacket + offset;
		for (unsigned int i = 0; i < len; ++i)
		{
			unsigned char* infAddr = InferiorContextMemoryAddress(addr + i);
			ExceptionSafeMemoryWrite(infAddr, HexToByte(ptr));
			ptr += 2;
		}
	}
	else
	{
		WriteError(1);
	}
}

void ReadMemory(void)
{
	unsigned char* addr;
	unsigned int len;
	short offset = GetAddressAndLength(1, false, &addr, &len);
	if (offset > 0)
	{
		for (unsigned int i = 0; i < len; ++i)
		{
			unsigned char* infAddr = InferiorContextMemoryAddress(addr + i);
			unsigned char membyte;
			ExceptionSafeMemoryRead(infAddr, &membyte);
			WriteByte(membyte);
		}
	}
	else
	{
		WriteError(1);
	}
}

// Note! Compares str_a with str_b *up to the length* of str_a.
// Returns: -1 if not equal, and length of str_a if equal.
// Returns 0 if str_a is length 0, and as such, is a dumb string to compare.
short StringCompare(const char* str_a, const char* str_b)
{
	short l = 0;
	while (str_a[l])
	{
		if (str_a[l] != str_b[l])
		{
			return -1;
		}
		++l;
	}
	return l;
}

void CmdQuery(void)
{
	short vNameEnd;
	if (StringCompare("qOffsets", inPacket) > 0)
	{
		/*
		As Gdb works with the elf file, and we have a prg file, the text and data segments do not match.
		However, we assume that the code was linked with one of the toolchains linker script.
		And then we can assume that the address of the __start symbol is the offset we need to tell gdb
		to make all symbols work.
		It just so happens that the __start symbol is always at inferiorBasePage->p_tbase, so all becomes easy.
		*/
		if (inferiorBasePage != NULL)	// Return empty if no inferior
		{
			unsigned int offset = (unsigned int)(inferiorBasePage->p_tbase);
			WriteNameAndLong("TextSeg", offset);
			WriteSemiColon();
			WriteNameAndLong("DataSeg", offset);
		}
	}
	else if (StringCompare("qSupported", inPacket) > 0)
	{
		// We don't care about what the gdb client supports, we just report back what we support.
		WriteString(serverFeatures);
	}
	else if (StringCompare("QStartNoAckMode", inPacket) > 0)
	{
		noAckMode = true;
		WriteOK();
	}
	else if ((vNameEnd = StringCompare("QSetWorkingDir:", inPacket)) > 0)
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
	else if ((vNameEnd = StringCompare("qXfer:features:read:target.xml:", inPacket)) > 0)
	{
		unsigned char* addr;
		unsigned int len;
		if (GetAddressAndLength(vNameEnd, false, &addr, &len) > 0)
		{
			unsigned int offset = (unsigned int)addr;
			unsigned int xml_len;
			char* xml = GetTargetXml(&xml_len);
			
			if ((offset + len) >= xml_len)
			{
				WriteChar('l');
				if (offset >= xml_len || len == 0)
				{
					return;
				}
				len = xml_len - offset;
			}
			else
			{
				WriteChar('m');
			}
			for (unsigned int i = 0; i < len; ++i)
			{
				WriteChar(xml[offset + i]);
			}
		}
	}
	else if ((vNameEnd = StringCompare("qXfer:features:read:", inPacket)) > 0)
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
	if (offset > 0 && inPacket[1] == '0')
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
	if (offset > 0 && inPacket[1] == '0')
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

LoopState CmdFlexible(void)
{
	short vNameEnd;

	if ((vNameEnd = StringCompare("vRun;",  inPacket)) > 0)
	{
		if (inferiorState != NOT_LOADED)
		{
			// Return error as we don't support multiple running executables.
			WriteError(1);
			return LISTEN_TO_GDB;
		}

		// Get filename and command line arguments
		// If filename is empty, then restart previous inferior.
		char* strOut = inferior_filename;
		short args = GetHexString(vNameEnd, &strOut);	// Will not replace old filename if no new exists
		if (args > vNameEnd + 1)
		{
			strOut = inferior_cmdline;
			while (args < inPacketLength)
			{
				args = GetHexString(args, &strOut);
				*strOut++ = ' ';
			}
			*strOut++ = 0;
		}
		DbgOut("Run: \r\n\tinferior: ");
		DbgOut(inferior_filename);
		DbgOut("\r\n\targs: ");
		DbgOut(inferior_cmdline);
		DbgOut(newline);
		return RUN;
	}
	else if ((vNameEnd = StringCompare("vKill;",  inPacket)) > 0)
	{
		return KILL;
	}
	return LISTEN_TO_GDB;
}

// We can get here in usermode, so should we make sure it is in supervisor, or check all calls we make?
void ServerCommandLoop(int si_signo, int si_code)
{
	DbgOut("ServerCommandLoop: \r\n");
	DbgOutVal("si_signo", (unsigned int)si_signo);
	DbgOutVal("si_code", (unsigned int)si_code);

	LoopState loopState = LISTEN_TO_GDB;
	EnableCtrlC(false); // We don't want any Ctrl-C breaking now.

	// Send response to GDB if needed.
	outPacketLength = 0;
	if (si_signo == GDB_SIGUSR1)
	{
		// We come here when called from ServerMain.
		// This is the only time when ServerCommandLoop isn't run in supervisor mode.
		if (si_code == USERCODE_ERROR)
		{
			WriteError(1);
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
		// An exception have occured.
		// We are in supervisor mode now.
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
		WriteStop(si_signo, si_code);
		TransmitPacket(false);
	}

	// Command loop, get packet from gdb and transmit response.
	while (loopState == LISTEN_TO_GDB)
	{
		bool skipAck = false;
		outPacketLength = 0;	// Keep track of the outPacket count and write pos
		ReceivePacket();
		switch (inPacket[0])
		{
		case 0x03:	// Ctrl-C. 
			// The user have requested to pause execution of inferior, but we are already paused or not even running...
			if (inferiorState == RUNNING)
			{
				// Just repeat the last stop code.
				WriteStop(si_signo, si_code);
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
		case 'D':	// Detach. Quit debugging but continue running inferior. Unhook exceptions?
			WriteOK();
			break;
		case 'H':	// Set thread number. Just return OK.
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
			if (inferiorState == NOT_LOADED)
			{
				if (option_multi)
				{
					// Need to load and start inferior
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
						TransmitPacket(skipAck);
					}
					skipAck = true; // Even for extended mode?				
				}
			}
			else if (inferiorState == RUNNING)
			{
				// We have indeed a running inferior and it have cast an exception.
				WriteStop(si_signo, si_code);
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
			break;
		case 'g':	// Get register values
			ReadRegisters();
			break;
		case 'G':	// Set register values
			WriteRegisters();
			break;
		case 'm':	// Read from memory
			ReadMemory();
			break;
		case 'M':	// Write to memory
			WriteMemory();
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
		EnableCtrlC(true);	// So we can break with Ctrl-C
		if (inferiorState == LOADED)
		{
			int return_code = 0;
			if (!RunInferior(&return_code))	// Returns after inferior is terminated.
			{
				// Inferior terminated itself. 
				// Report process exit and inferior return code to gdb.
				outPacketLength = 0;
				WriteChar('W');
				WriteByte((unsigned char)return_code);
				TransmitPacket(true);	// We don't expect any answer.
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
	DbgOut("ServerCommandLoop: exiting\r\n");
}

#pragma GCC diagnostic push
// Remove out of bounds warning, as the cookie code will trigger false warnings.
#pragma GCC diagnostic ignored "-Warray-bounds="

// Find out what kind of system we are running on.
int GetCookies(void)
{
	DbgOut("Using system cookies:\r\n");
	
	struct cookie
	{
		unsigned int cookie;
		unsigned int value;
	};
	#define COOKIE_NAME(a,b,c,d) ((a<<24)|(b<<16)|(c<<8)|(d))
	struct cookie* _p_cookies = ((struct cookie**)0x5a0)[0];
	if (_p_cookies != NULL)
	{
		while (_p_cookies->cookie != 0)
		{
			switch(_p_cookies->cookie)
			{
				case COOKIE_NAME('_', 'C', 'P', 'U'):
					Cookie_CPU = _p_cookies->value;
					DbgOutVal("\t_CPU", Cookie_CPU);
					break;
				case COOKIE_NAME('_', 'V', 'D', 'O'):
					Cookie_VDO = _p_cookies->value;
					DbgOutVal("\t_VDO", Cookie_VDO);
					break;
				case COOKIE_NAME('_', 'F', 'P', 'U'):
					Cookie_FPU = _p_cookies->value;
					DbgOutVal("\t_FPU", Cookie_FPU);
					break;
				case COOKIE_NAME('_', 'M', 'C', 'H'):
					Cookie_MCH = _p_cookies->value;
					DbgOutVal("\t_MCH", Cookie_MCH);
					break;
			}
			++_p_cookies;
		}
	}
	#undef COOKIE_NAME
	return 0;
}
#pragma GCC diagnostic pop

/*
	Option handling for this server is made to follow the real gdbserver documentation.
	However, most of the options for the real gdbserver is not applicable for us.

	gdbsrv.ttp	[options] [comm] prog [args]

	comm	(Default if missing: COM1)
		COM[n]	Where n is the com port number to use.
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
	loadInferiorRequested = false;
	char *inferior_args = inferior_cmdline;
	inferior_args[0] = 0;
	if (argc <= 1)
	{
		DbgOut("\tNo args.\r\n");
		inferior_filename[0] = 0;
		inferior_cmdline[0] = 0;
		return result;
	}
	for (int i = 1; i < argc; ++i)
	{
		if (loadInferiorRequested)
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
				if (StringCompare("--debug-remote", argv[i]) > 0)
				{
					log_debug_remote = true;
					DbgOut("Using: --debug-remote\r\n");
				}
				else if (StringCompare("--debug", argv[i]) > 0)
				{
					log_debug = true;
					DbgOut("Using: --debug\r\n");
				}
				else if (StringCompare("--multi", argv[i]) > 0)
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
				loadInferiorRequested = true;
				DbgOut("Inferior file: ");
				DbgOut(argv[i]);
				DbgOut(newline);
			}
		}
	}
	if (option_multi)
	{
		loadInferiorRequested = false;	// Never load inferior directly when multi.
	}
	if (inferior_args != inferior_cmdline && inferior_args[-1] == ' ')
	{
		inferior_args[-1] = 0;
	}

	return result;
}

int ServerMain(int argc, char** argv)
{
	DbgOut("ServerMain: Server initing.\r\n");
	if (HandleOptions(argc, argv) != 0)
	{
		return -1;
	}
	int ret = -1;
	// Get cookies
	Supexec(GetCookies);
	if ((Cookie_FPU & (0x1f << 16)) != 0 && Cookie_CPU >= 20)
	{
		numOfCpuRegisters = 29;
	}
	// Set serial conf
	Rsconf(BAUD_9600, FLOW_HARD, RS_CLK16 | RS_1STOP | RS_8BITS, RS_INQUIRE, RS_INQUIRE, RS_INQUIRE);
	// Empty serial buffer
	while (Bconstat(DEV_AUX) != 0)
	{
		Bconin(DEV_AUX);
	}
	
	// Set DTR to ON
	Ongibit(GI_DTR);
	CreateServerContext();
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
			if (LoadInferior(inferior_filename, inferior_cmdline, NULL) < 0)
			{
				DbgOut("Could not load inferior: ");
				DbgOut(inferior_filename);
				DbgOut(newline);
				
				if (userCodeForCommandLoop == USERCODE_REPORT)
				{
					userCodeForCommandLoop = USERCODE_ERROR;
				}
				else
				{
					// Exit gdbserver if *command line* specified inferior could not be loaded.
					break;
				}
			}
			loadInferiorRequested = false;
		}
		ServerCommandLoop(GDB_SIGUSR1, userCodeForCommandLoop);
		DiscardAllBreakpoints();
		ret = 0;
	} while ((extendedMode && !run_once) || option_multi);
	DestroyServerContext();
	// Set DTR to OFF
	Offgibit(GI_DTR);
	
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

