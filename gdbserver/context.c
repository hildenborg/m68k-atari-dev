#include "context.h"
#include "bios_calls.h"
#include "server.h"
#include "exceptions.h"
#include "critical.h"

#define NUM_IRQ_VECTORS 8
#define NUM_MFP_VECTORS 16
#define NUM_VDO_LONGS 8
#define NUM_SYS_LONGS 1
#define NUM_VDO_BYTES 6
#define NUM_MFP_BYTES 18

// Server context
unsigned int serverVectors[NUM_IRQ_VECTORS + NUM_MFP_VECTORS];
unsigned int serverLongs[NUM_VDO_LONGS + NUM_SYS_LONGS];
unsigned char serverBytes[NUM_VDO_BYTES + NUM_MFP_BYTES] __attribute__((aligned(2)));

// Inferior context
unsigned int inferiorVectors[NUM_IRQ_VECTORS + NUM_MFP_VECTORS];
unsigned int inferiorLongs[NUM_VDO_LONGS + NUM_SYS_LONGS];
unsigned char inferiorBytes[NUM_VDO_BYTES + NUM_MFP_BYTES] __attribute__((aligned(2)));

void StoreVectors(unsigned int* vectors);
void RestoreVectors(unsigned int* vectors);
void StoreMemoryRegisters(unsigned int* longs, unsigned char* bytes);
void RestoreMemoryRegisters(unsigned int* longs, unsigned char* bytes);

int CreateServerContext_super(void)
{
	InitExceptions();
	ClearInternalCaches();
    // Store server context.
    StoreVectors(serverVectors);
    StoreMemoryRegisters(serverLongs, serverBytes);
    return 0;
}

void CreateServerContext(void)
{
	DiscardAllBreakpoints();
	Supexec(CreateServerContext_super);
}

void DestroyServerContext(void)
{
    // No need to restore server context as we will not use it again.
	Supexec(RestoreExceptions);
}

void SwitchToInferiorContext(void)
{
    // Restore inferior context that was stored in SwitchToServerContext
    RestoreVectors(inferiorVectors);
    RestoreMemoryRegisters(inferiorLongs, inferiorBytes);
	ClearInternalCaches();
}

void SwitchToServerContext(void)
{
    // Store current inferior context.
    StoreVectors(inferiorVectors);
    StoreMemoryRegisters(inferiorLongs, inferiorBytes);
    // Restore original server context.
    RestoreVectors(serverVectors);
    RestoreMemoryRegisters(serverLongs, serverBytes);
	ClearInternalCaches();
}

#pragma GCC diagnostic push
// Remove out of bounds warning, as we do some memory direct stuff here that will trigger false warnings.
#pragma GCC diagnostic ignored "-Warray-bounds="

void StoreVectors(unsigned int* vectors)
{
	unsigned int* vector60 = (unsigned int*)0x60;
	for (int i = 0; i < NUM_IRQ_VECTORS; ++i)
	{
		*vectors++ = vector60[i];
	}
	unsigned int* vector100 = (unsigned int*)0x100;
	for (int i = 0; i < NUM_MFP_VECTORS; ++i)
	{
		*vectors++ = vector100[i];
	}
}

void RestoreVectors(unsigned int* vectors)
{
	unsigned int* vector60 = (unsigned int*)0x60;
	for (int i = 0; i < NUM_IRQ_VECTORS; ++i)
	{
		vector60[i] = *vectors++;
	}
	unsigned int* vector100 = (unsigned int*)0x100;
	for (int i = 0; i < NUM_MFP_VECTORS; ++i)
	{
		vector100[i] = *vectors++;
	}
}

void StoreMemoryRegisters(unsigned int* longs, unsigned char* bytes)
{
	unsigned int* sysLongs = longs;
	unsigned int* vdoLongs = longs + NUM_SYS_LONGS;
	unsigned char* vdoChars = bytes;
	//unsigned char* mfpChars = bytes + NUM_VDO_BYTES;
	#define SYS_LONG(address) *sysLongs++ = *((unsigned int*)address)
	#define VDO_LONG(address) *vdoLongs++ = *((unsigned int*)address)
	#define VDO_CHAR(address) *vdoChars++ = *((unsigned char*)address)
	//#define MFP_CHAR(address) *mfpChars++ = *((unsigned char*)address)

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
	/*
	for (unsigned int i = 0xfffffa03; i <= 0xfffffa1d; i += 2)
	{
		MFP_CHAR(i);
	}
	for (unsigned int i = 0xfffffa1f; i <= 0xfffffa25; i += 2)
	{
		*mfpChars++ = CaptureMfpData((unsigned char*)i);
	}
	*/

	SYS_LONG(0x44e);
	
	#undef SYS_LONG
	#undef VDO_LONG
	#undef VDO_CHAR
	//#undef MFP_CHAR
}

void RestoreMemoryRegisters(unsigned int* longs, unsigned char* bytes)
{
	unsigned int* sysLongs = longs;
	unsigned int* vdoLongs = longs + NUM_SYS_LONGS;
	unsigned char* vdoChars = bytes;
	//unsigned char* mfpChars = bytes + NUM_VDO_BYTES;
	#define SYS_LONG(address) *((unsigned int*)address) = *sysLongs++
	#define VDO_LONG(address) *((unsigned int*)address) = *vdoLongs++
	#define VDO_CHAR(address) *((unsigned char*)address) = *vdoChars++
//	#define MFP_CHAR(address) *((unsigned char*)address) = *mfpChars++

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
	/*
	for (unsigned int i = 0xfffffa03; i <= 0xfffffa25; i += 2)
	{
		MFP_CHAR(i);
	}
	*/
	SYS_LONG(0x44e);
	
	#undef SYS_LONG
	#undef VDO_LONG
	#undef VDO_CHAR
//	#undef MFP_CHAR
}


// This method make sure to return a pointer to the data that would have been used by the inferior,
// and not the one currently used by the server.
// The addresses comes from gdb, and never from the inferior.
// However, we still cannot trust that the addresses are "clean" and do not use the upper 8 bits
// on a 24bit address bus.
unsigned char* InferiorContextMemoryAddress(unsigned char* address)
{
	unsigned int la = (unsigned int)address;
	if (Cookie_CPU < 20)
	{
		// Need to properly set the upper 8 bits.
		la = (unsigned int)(((int)la << 8) >> 8);
	}
	if (la < 0x8000)
	{
		// 0x0000.w to 0x7fff.w
		if (la >= 0x60 && la < 0x80)
		{
            unsigned char* inferiorVectors60 = (unsigned char*)(inferiorVectors);
			return &inferiorVectors60[la - 0x60];
		}
		else if (la >= 0x100 && la < 0x140)
		{
            unsigned char* inferiorVectors100 = (unsigned char*)(inferiorVectors + NUM_IRQ_VECTORS);
			return &inferiorVectors100[la - 0x100];
		}
		else if (la >= 0x44e && la < 0x450)
		{
            unsigned char* inferiorSysLongs = (unsigned char*)(inferiorLongs);
			return &inferiorSysLongs[la - 0x44e];
		}
	}
	else if (la >= 0xffff8000)
	{
		// 0xffff8000.w to 0xffffffff.w
		if (la >= 0xffff8240 && la < 0xffff8260)
		{
            unsigned char* inferiorVdoLongs = (unsigned char*)(inferiorLongs + NUM_SYS_LONGS);
			return &inferiorVdoLongs[la - 0xffff8240];
		}
        /*
		else if (la >= 0xfffffa03 && la <= 0xfffffa25 && ((la & 1) != 0))
		{
			return &((unsigned char*)inferiorMfpChars)[(la - 0xfffffa03) >> 1];
		}
        */
		else
		{
            unsigned char* inferiorVdoChars = (inferiorBytes);
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

#pragma GCC diagnostic pop
