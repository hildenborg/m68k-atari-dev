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

bool IsServerException(void);
int InsertMemoryBreakpoint(unsigned short* addr);
int RemoveMemoryBreakpoint(unsigned short* addr);
int IsBreakpoint(unsigned short* addr);
ExceptionRegisters* GetRegisters(void);
void EnableCtrlC(bool onOff);
bool GetDCD(void);

#ifdef __cplusplus
}
#endif

#endif // EXCEPTIONS_DEFINED
