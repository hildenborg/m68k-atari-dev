/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#ifndef EXCEPTIONS_DEFINED
#define EXCEPTIONS_DEFINED

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct
{
	unsigned int	d0, d1, d2, d3, d4, d5, d6, d7;
	unsigned int	a0, a1, a2, a3, a4, a5, a6, sp;
	unsigned int	sr, pc;
	unsigned int	fp0[3], fp1[3], fp2[3], fp3[3], fp4[3], fp5[3], fp6[3], fp7[3];
	unsigned int	fpControl, fpStatus, fpIAddr;
} ExceptionRegisters;

void Exception(void);
void DiscardAllBreakpoints(void);

struct BasePage;
// True if the stopped pc is in the text segment of the program with this basepage. False for NULL.
bool IsPcInText(const struct BasePage* basePage);
bool IsServerException(void);
int InsertMemoryBreakpoint(unsigned short* addr);
int RemoveMemoryBreakpoint(unsigned short* addr);
int IsBreakpoint(unsigned short* addr);
ExceptionRegisters* GetRegisters(void);

// Set when the server sets the trace bit in sr to single step the inferior.
// The next exception then removes the bit from the reported sr.
extern bool serverTraceBit;

/*
	True while the inferior is stopped somewhere that may be inside GEMDOS: in supervisor mode,
	outside the inferior's own code, e.g. by Ctrl-C during a GEMDOS call or a step into a trap.
	GEMDOS is not reentrant, so the server must not call it then.
	A stop in the inferior's own interrupt handler, that interrupted a GEMDOS call, is not detected.
*/
extern bool osMaybeBusy;

#ifdef __cplusplus
}
#endif

#endif // EXCEPTIONS_DEFINED
