/*
	Copyright (C) 2026 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#ifndef COOKIES_DEFINED
#define COOKIES_DEFINED

/* 
	_CPU: 
	0 = 68000
	10 = 68010
	20 = 68020
	30 = 68030
	40 = 68040
	60 = 68060
*/
extern unsigned int Cookie_CPU;
/*
	_VDO (shifter [high word, low word]):
	[0,0] = st
	[1,0] = ste
	[2,0] = tt
	[3,0] = falcon
*/
extern unsigned int Cookie_VDO;
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
extern unsigned int Cookie_FPU;
/*
	_MCH (machine [high word, low word]):
	[0,0] = st
	[1,0] = ste
	[1,8] = st book
	[1,16] mega ste
	[2,0] = tt
	[3,0] = falcon
*/
extern unsigned int Cookie_MCH;

/*
	If this cookie is set, then it will point to a comm structure with
	all the callbacks necessary for communication with gdb.
*/
extern unsigned int Cookie_SDBG;


int GetCookies(void);

#endif // COOKIES_DEFINED
