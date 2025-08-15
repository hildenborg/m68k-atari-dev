#ifndef EXCEPTIONS_DEFINED
#define EXCEPTIONS_DEFINED

#include <signal.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	unsigned int	exponent;	// lower 16 bits are always zero and unused.
	unsigned int	mantissa_high;
	unsigned int	mantissa_low;
}  float96;

typedef struct
{
	unsigned int	d0, d1, d2, d3, d4, d5, d6, d7;
	unsigned int	a0, a1, a2, a3, a4, a5, a6, sp;
	unsigned int	sr, pc;
	/*
		Fix for FPU later:
	unsigned char	fp0[12], fp1[12], fp2[12], fp3[12], fp4[12], fp5[12], fp6[12], fp7[12];
	unsigned int	fpControl, fpStatus, fpIAddr;
	*/
} ExceptionRegisters;

int InitExceptions(void);
void RestoreExceptions(void);
void Exception(int num);
void DiscardAllBreakpoints(void);

bool IsServerException(void);
int InsertMemoryBreakpoint(unsigned short* addr);
int RemoveMemoryBreakpoint(unsigned short* addr);
int IsBreakpoint(unsigned short* addr);
ExceptionRegisters* GetRegisters(void);
void EnableCtrlC(bool onOff);
bool GetDCD(void);

unsigned char* GetInferiorMemoryAddress(unsigned char* address);

// exceptions_asm.s
int ASM_InitExceptions(void);
int ASM_RestoreExceptions(void);

// These functions assume that we are in Supervisor mode.
// Returns non zero if exception occured.
int ASM_ExceptionSafeMemoryRead(unsigned char* address, unsigned char* c);
int ASM_ExceptionSafeMemoryWrite(unsigned char* address, unsigned char c);

unsigned char ASM_CaptureMfpData(unsigned char* address);

#ifndef BUS_ADRALN
#define BUS_ADRALN	1
#endif

#ifndef BUS_ADRERR
#define BUS_ADRERR	2
#endif

#ifndef ILL_ILLOPC
#define ILL_ILLOPC	3
#endif

#ifndef FPE_INTDIV
#define FPE_INTDIV	4
#endif

#ifndef FPE_INTOVF
#define FPE_INTOVF	5
#endif

#ifndef ILL_PRVOPC
#define ILL_PRVOPC	6
#endif

#ifndef TRAP_TRACE
#define TRAP_TRACE	7
#endif

#ifndef BUS_OBJERR
#define BUS_OBJERR	8
#endif

#ifndef TRAP_BRKPT
#define TRAP_BRKPT	9
#endif

#ifdef __cplusplus
}
#endif

#endif // EXCEPTIONS_DEFINED
