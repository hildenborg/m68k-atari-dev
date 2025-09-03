/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

/*
    Supervisor mode is assumed for all functions!
*/
#ifndef CRITICAL_DEFINED
#define CRITICAL_DEFINED

int InitExceptions(void);
int RestoreExceptions(void);

void ClearInternalCaches(void);

unsigned char CaptureMfpData(unsigned char* address);

int ExceptionSafeMemoryRead(unsigned char* address, unsigned char* c);
int ExceptionSafeMemoryWrite(unsigned char* address, unsigned char c);

// Supervisor mode not needed.
int GetExceptionNum(void);

void CtrlCException(void);

#endif // CRITICAL_DEFINED
