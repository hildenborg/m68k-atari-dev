#include "bios_calls.h"
#include "exceptions.h"
#include "server.h"

#define BREAKPOINT	0x4e40		// Trap #0
#define NUM_MEMPOINTS 128		// Max number of breakpoints handled by this code.

ExceptionRegisters registers;

unsigned int serverVectors60[8];
unsigned int inferiorVectors60[8];
unsigned int serverVectors100[16];
unsigned int inferiorVectors100[16];
unsigned int serverVdoLongs[8];
unsigned int inferiorVdoLongs[8];
unsigned int serverSysLongs[1];
unsigned int inferiorSysLongs[1];
unsigned char serverVdoChars[6] __attribute__((aligned(2)));
unsigned char inferiorVdoChars[6] __attribute__((aligned(2)));
unsigned char serverMfpChars[18] __attribute__((aligned(2)));
unsigned char inferiorMfpChars[18] __attribute__((aligned(2)));

volatile short CtrlC_enable = 0;
volatile unsigned char Mfp_ActiveEdgeRegister __attribute__((aligned(2))) = 0;

typedef struct
{
	unsigned short* addr;
	unsigned short store;
} MemBreak;

MemBreak mempoints[NUM_MEMPOINTS];

extern volatile struct BasePage*	inferiorBasePage;

extern void DbgOutVal(const char* name, unsigned int val);


int InitExceptions(void)
{
	DiscardAllBreakpoints();
	return Supexec(ASM_InitExceptions);
}

void RestoreExceptions(void)
{
	Supexec(ASM_RestoreExceptions);
}

void EnableCtrlC(bool onOff)
{
	CtrlC_enable = onOff ? 1 : 0;
}

bool GetDCD(void)
{
	return (Mfp_ActiveEdgeRegister & 0x02) == 0;
}

// Check pc to see if it is in server code.
bool IsServerException(void)
{
	struct BasePage* serverBasePage = _BasePage; // Defined in crt0
	unsigned int textStart = (unsigned int)(serverBasePage->p_tbase);
	unsigned int textEnd = textStart + serverBasePage->p_tlen;
	return textStart <= registers.pc && textEnd > registers.pc;
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
	// Do a sanity check and only allow setting breakpoints in even memory owned by inferior.
	unsigned int iaddr = (unsigned int)addr;
	unsigned int textStart = (unsigned int)(inferiorBasePage->p_tbase);
	unsigned int bssEnd = (unsigned int)(inferiorBasePage->p_bbase) + inferiorBasePage->p_blen;
	if ((iaddr & 0x1) != 0 || iaddr < textStart || iaddr > bssEnd)
	{
		return -1;
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
	for (int i = 0; i < NUM_MEMPOINTS; ++i)
	{
		if (mempoints[i].addr == addr)
		{
			mempoints[i].addr = 0;
			*addr = mempoints[i].store;
			return 0;
		}
	}
	return -1;
}

int IsBreakpoint(unsigned short* addr)
{
	for (short i = 0; i < NUM_MEMPOINTS; ++i)
	{
		if (mempoints[i].addr == addr)
		{
			return i;
		}
	}
	return -1;
}

void Exception(int num)
{
	int si_signo = SIGINT;
	int si_code = 0;
	switch (num)
	{
		case 2:		// BusError
			si_signo = SIGBUS;
			si_code = BUS_ADRALN;
			break;
		case 3:		// AddressError
			si_signo = SIGBUS;
			si_code = BUS_ADRERR;
			break;
		case 4:		// IllegalInstruction
			si_signo = SIGILL;
			si_code = ILL_ILLOPC;
			break;
		case 5:		// DivisionByZero
			si_signo = SIGFPE;
			si_code = FPE_INTDIV;
			break;
		/*
		case 6:		// CHK
			si_signo = SIGFPE;
			si_code = ;
			break;
		*/
		case 7:		// TrapV
			si_signo = SIGFPE;
			si_code = FPE_INTOVF;
			break;
		case 8:		// PrivilegeViolation
			si_signo = SIGILL;
			si_code = ILL_PRVOPC;
			break;
		case 9:		// Trace
			si_signo = SIGTRAP;
			si_code = TRAP_TRACE;
			break;
		case 31:	// NMI
			si_signo = SIGBUS;
			si_code = BUS_OBJERR;
			break;
		case 32:	// BreakPoint
			{
				si_signo = SIGTRAP;
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
		case 0x4c:	// SerialInput CTRL-C
			si_signo = SIGINT;
			si_code = 0;
			break;
		default:	// Anything else.
			si_signo = SIGINT;
			si_code = 0;
			break;
	}
	ServerCommandLoop(si_signo, si_code);
}

ExceptionRegisters* GetRegisters(void)
{
	return & registers;
}

// If inferior == 0 then it is the server vectors, otherwise it's the inferiors.
void StoreVectors(unsigned int inferior)
{
	unsigned int* storage60 = inferior == 0 ? serverVectors60 : inferiorVectors60;
	unsigned int* vector60 = (unsigned int*)0x60;
	for (int i = 0; i < 8; ++i)
	{
		storage60[i] = vector60[i];
	}
	unsigned int* storage100 = inferior == 0 ? serverVectors100 : inferiorVectors100;
	unsigned int* vector100 = (unsigned int*)0x100;
	for (int i = 0; i < 16; ++i)
	{
		storage100[i] = vector100[i];
	}
}

// If inferior == 0 then it is the server vectors, otherwise it's the inferiors.
void LoadVectors(unsigned int inferior)
{
	unsigned int* storage60 = inferior == 0 ? serverVectors60 : inferiorVectors60;
	unsigned int* vector60 = (unsigned int*)0x60;
	for (int i = 0; i < 8; ++i)
	{
		vector60[i] = storage60[i];
	}
	unsigned int* storage100 = inferior == 0 ? serverVectors100 : inferiorVectors100;
	unsigned int* vector100 = (unsigned int*)0x100;
	for (int i = 0; i < 16; ++i)
	{
		vector100[i] = storage100[i];
	}
}

// If inferior == 0 then it is the server memory, otherwise it's the inferiors.
void StoreMemoryRegisters(unsigned int inferior)
{
	unsigned int* sysLongs = inferior == 0 ? serverSysLongs : inferiorSysLongs;
	unsigned int* vdoLongs = inferior == 0 ? serverVdoLongs : inferiorVdoLongs;
	unsigned char* vdoChars = inferior == 0 ? serverVdoChars : inferiorVdoChars;
	unsigned char* mfpChars = inferior == 0 ? serverMfpChars : inferiorMfpChars;
	#define SYS_LONG(address) *sysLongs++ = *((unsigned int*)address)
	#define VDO_LONG(address) *vdoLongs++ = *((unsigned int*)address)
	#define VDO_CHAR(address) *vdoChars++ = *((unsigned char*)address)
	#define MFP_CHAR(address) *mfpChars++ = *((unsigned char*)address)

	VDO_CHAR(0xffff8201);
	VDO_CHAR(0xffff8203);
	VDO_CHAR(0xffff820a);
	VDO_CHAR(0xffff8260);
	if (Cookie_VDO == 0x10000)
	{
		// Got STE shifter
		VDO_CHAR(0xffff820d);
		VDO_CHAR(0xffff8265);
	}
	for (unsigned int i = 0xffff8240; i < 0xffff8260; i += 4)
	{
		VDO_LONG(i);
	}
	for (unsigned int i = 0xfffffa03; i <= 0xfffffa1d; i += 2)
	{
		MFP_CHAR(i);
	}
	for (unsigned int i = 0xfffffa1f; i <= 0xfffffa25; i += 2)
	{
		*mfpChars++ = ASM_CaptureMfpData((unsigned char*)i);
	}

	SYS_LONG(0x44e);
	
	#undef SYS_LONG
	#undef VDO_LONG
	#undef VDO_CHAR
	#undef MFP_CHAR
}

// If inferior == 0 then it is the server memory, otherwise it's the inferiors.
void LoadMemoryRegisters(unsigned int inferior)
{
	unsigned int* sysLongs = inferior == 0 ? serverSysLongs : inferiorSysLongs;
	unsigned int* vdoLongs = inferior == 0 ? serverVdoLongs : inferiorVdoLongs;
	unsigned char* vdoChars = inferior == 0 ? serverVdoChars : inferiorVdoChars;
	unsigned char* mfpChars = inferior == 0 ? serverMfpChars : inferiorMfpChars;
	#define SYS_LONG(address) *((unsigned int*)address) = *sysLongs++
	#define VDO_LONG(address) *((unsigned int*)address) = *vdoLongs++
	#define VDO_CHAR(address) *((unsigned char*)address) = *vdoChars++
	#define MFP_CHAR(address) *((unsigned char*)address) = *mfpChars++

	VDO_CHAR(0xffff8201);
	VDO_CHAR(0xffff8203);
	VDO_CHAR(0xffff820a);
	VDO_CHAR(0xffff8260);
	if (Cookie_VDO == 0x10000)
	{
		// Got STE shifter
		VDO_CHAR(0xffff820d);
		VDO_CHAR(0xffff8265);
	}
	for (unsigned int i = 0xffff8240; i < 0xffff8260; i += 4)
	{
		VDO_LONG(i);
	}
	for (unsigned int i = 0xfffffa03; i <= 0xfffffa25; i += 2)
	{
		MFP_CHAR(i);
	}

	SYS_LONG(0x44e);
	
	#undef SYS_LONG
	#undef VDO_LONG
	#undef VDO_CHAR
	#undef MFP_CHAR
}

// This method make sure to return apointer to the data that would have been used by the inferior,
// and not the one currently used by the server.
unsigned char* GetInferiorMemoryAddress(unsigned char* address)
{
	unsigned int la = (unsigned int)address;
	if (la < 0x8000)
	{
		// 0x0000.w to 0x7fff.w
		if (la >= 0x60 && la < 0x80)
		{
			return &((unsigned char*)inferiorVectors60)[la - 0x60];
		}
		else if (la >= 0x100 && la < 0x140)
		{
			return &((unsigned char*)inferiorVectors100)[la - 0x100];
		}
		else if (la >= 0x44e && la < 0x450)
		{
			return &((unsigned char*)inferiorSysLongs)[la - 0x44e];
		}
	}
	else if (la >= 0xffff8000)
	{
		// 0xffff8000.w to 0xffffffff.w
		if (la >= 0xffff8240 && la < 0xffff8260)
		{
			return &((unsigned char*)inferiorVdoLongs)[la - 0xffff8240];
		}
		else if (la >= 0xfffffa03 && la <= 0xfffffa25 && ((la & 1) != 0))
		{
			return &((unsigned char*)inferiorMfpChars)[(la - 0xfffffa03) >> 1];
		}
		else
		{
			switch (la)
			{
				case 0xffff8201:	return &inferiorVdoChars[0];
				case 0xffff8203:	return &inferiorVdoChars[1];
				case 0xffff820a:	return &inferiorVdoChars[2];
				case 0xffff8260:	return &inferiorVdoChars[3];
				case 0xffff820d:	return &inferiorVdoChars[4];
				case 0xffff8265:	return &inferiorVdoChars[5];
				default:			return address;
			}
		}
	}
	// Any address not covered by above if's
	return address;
}
