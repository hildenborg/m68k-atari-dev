/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#include <stddef.h>
#include "gdb_defines.h"
#include "bios_calls.h"
#include "exceptions.h"
#include "server.h"
#include "critical.h"
#include "inferior.h"

#define BREAKPOINT	0x4e40		// Trap #0
#define NUM_MEMPOINTS 32		// Max number of breakpoints handled by this code.

ExceptionRegisters registers;
bool serverTraceBit = false;
unsigned int rangeStepStart = 0;
unsigned int rangeStepEnd = 0;
unsigned short ctrlCPending = 0;
bool osMaybeBusy = false;

typedef struct
{
	unsigned short* addr;
	unsigned short store;
} MemBreak;

MemBreak mempoints[NUM_MEMPOINTS];

bool IsPcInText(const struct BasePage* basePage)
{
	if (basePage == NULL)
	{
		return false;
	}
	unsigned int textStart = (unsigned int)(basePage->p_tbase);
	unsigned int textEnd = textStart + basePage->p_tlen;
	return textStart <= registers.pc && textEnd > registers.pc;
}

// Check pc to see if it is in server code.
bool IsServerException(void)
{
	return IsPcInText(_BasePage);	// Defined in crt0
}

void DiscardAllBreakpoints(void)
{
	for (int i = 0; i < NUM_MEMPOINTS; ++i)
	{
		mempoints[i].addr = 0;
	}
}

int InsertMemoryBreakpoint(unsigned short* addr)
{
	if (inferiorBasePage == NULL)
	{
		return -1;	// No inferior loaded.
	}
	// Do a sanity check and only allow setting breakpoints in even memory owned by inferior.
	unsigned int iaddr = (unsigned int)addr;
	unsigned int textStart = (unsigned int)(inferiorBasePage->p_tbase);
	unsigned int bssEnd = (unsigned int)(inferiorBasePage->p_bbase) + inferiorBasePage->p_blen;
	if ((iaddr & 0x1) != 0 || iaddr < textStart || iaddr + 2 > bssEnd)
	{
		return -1;
	}
	if (IsBreakpoint(addr) >= 0)
	{
		// Already set. Inserting it again would store the trap instruction as the original word.
		return 0;
	}
	for (int i = 0; i < NUM_MEMPOINTS; ++i)
	{
		if (mempoints[i].addr == 0)
		{
			mempoints[i].addr = addr;
			mempoints[i].store = *addr;
			*addr = BREAKPOINT;
			return 0;
		}
	}
	return -1;
}

int RemoveMemoryBreakpoint(unsigned short* addr)
{
	short i = IsBreakpoint(addr);
	if (i < 0)
	{
		return -1;
	}
	mempoints[i].addr = 0;
	if (inferiorBasePage != NULL)
	{
		// Only restore the word while the inferior still owns the memory.
		*addr = mempoints[i].store;
	}
	return 0;
}

int IsBreakpoint(unsigned short* addr)
{
	if (addr == NULL)
	{
		return -1;	// NULL marks an empty slot.
	}
	for (short i = 0; i < NUM_MEMPOINTS; ++i)
	{
		if (mempoints[i].addr == addr)
		{
			return i;
		}
	}
	return -1;
}

void Exception(void)
{
	// Ctrl-C (CtrlCException, number 1000) and any exception not listed below report SIGINT.
	int si_signo = GDB_SIGINT;
	int si_code = 0;
	rangeStepEnd = 0;	// The inferior has stopped, so any range step is over.
	bool ctrlC = ctrlCPending != 0;
	ctrlCPending = 0;
	if (serverTraceBit)
	{
		// The stacked sr still has the trace bit the server set for a step, both after the
		// trace exception and when the stepped instruction faulted. It is not the inferior's.
		serverTraceBit = false;
		registers.sr &= ~0x8000;
	}
	switch (GetExceptionNum())
	{
		case 2:		// BusError
			si_signo = GDB_SIGBUS;
			si_code = BUS_ADRALN;
			break;
		case 3:		// AddressError
			si_signo = GDB_SIGBUS;
			si_code = BUS_ADRERR;
			break;
		case 4:		// IllegalInstruction
			si_signo = GDB_SIGILL;
			si_code = ILL_ILLOPC;
			break;
		case 5:		// DivisionByZero
			si_signo = GDB_SIGFPE;
			si_code = FPE_INTDIV;
			break;
		case 7:		// TrapV
			si_signo = GDB_SIGFPE;
			si_code = FPE_INTOVF;
			break;
		case 8:		// PrivilegeViolation
			si_signo = GDB_SIGILL;
			si_code = ILL_PRVOPC;
			break;
		case 9:		// Trace
			if (ctrlC)
			{
				// Ctrl-C during range stepping, deferred to this trace exception.
				si_signo = GDB_SIGINT;
				si_code = 0;
			}
			else
			{
				si_signo = GDB_SIGTRAP;
				si_code = TRAP_TRACE;
			}
			break;
		case 31:	// NMI
			si_signo = GDB_SIGBUS;
			si_code = BUS_OBJERR;
			break;
		case 32:	// BreakPoint
			{
				si_signo = GDB_SIGTRAP;
				si_code = TRAP_BRKPT;
				/*
					When we set a breakpoint, we replace a word at the specified address with trap #0.
					When that is executed, we get here.
					So to keep executing after the breakpoint, GDB will ask us to remove the breakpoint.
					This means that to execute the original instruction that was replaced by trap #0, we must
					subtract 2 from pc so the original instruction gets executed.
					
					However, if the trap #0 wasn't a breakpoint set by GDB, then it may be the inferior code that
					have a trap #0 compiled into code (user compiled breakpoint). In that case, pc should not be altered,
					but execution must continue after the trap #0 when returning from debugger.
				*/
				if (IsBreakpoint((unsigned short*)(registers.pc - 2)) >= 0)
				{
					registers.pc -= 2;
				}
				
			}
			break;
		case 48:	// FPUnordered
			si_signo = GDB_SIGFPE;
			si_code = FPE_FLTINV;
			break;
		case 49:	// FPInexact
			si_signo = GDB_SIGFPE;
			si_code = FPE_FLTRES;
			break;
		case 50:	// FPDivideZero
			si_signo = GDB_SIGFPE;
			si_code = FPE_FLTDIV;
			break;
		case 51:	// FPUnderflow
			si_signo = GDB_SIGFPE;
			si_code = FPE_FLTUND;
			break;
		case 52:	// FPOperandError
			si_signo = GDB_SIGFPE;
			si_code = FPE_FLTINV;
			break;
		case 53:	// FPOverflow
			si_signo = GDB_SIGFPE;
			si_code = FPE_FLTOVF;
			break;
		case 54:	// FPSignalingNAN
			si_signo = GDB_SIGFPE;
			si_code = FPE_FLTINV;
			break;
		case 55:	// FPUnimplemented
			si_signo = GDB_SIGFPE;
			si_code = FPE_FLTINV;
			break;
	}
	ServerCommandLoop(si_signo, si_code);
}

ExceptionRegisters* GetRegisters(void)
{
	return & registers;
}

