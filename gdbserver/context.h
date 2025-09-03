/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#ifndef CONTEXT_DEFINED
#define CONTEXT_DEFINED

// Sets up all vectors and registers for running the server.
int CreateServerContext(void);

// Cleans up all vectors and registers altered by CreateServerContext.
void DestroyServerContext(void);

// Enters inferor context.
// Needs IRQ off and supervisor mode!
void SwitchToInferiorContext(void);

// Enters server context.
// Needs IRQ off and supervisor mode!
void SwitchToServerContext(void);

// Returns a pointer to either the same address or a shadow address containing the inferior data.
unsigned char* InferiorContextMemoryAddress(unsigned char* address);


#endif // CONTEXT_DEFINED
