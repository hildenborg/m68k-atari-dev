/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

/*
	GCC will optimize some c code into clib functions.
	So this code:
		size_t len = 0;
		while (s[len] != 0) {++len;}
	Will become:
		size_t len = strlen(s);

	But we do not have any c libraries so the linker will fail after gcc have optimized the code...
	So we have to supply our own assembler functions to make the gcc optimizations work.
	It must be assembler or gcc will optimize the code again...
*/

#include "clib.h"

size_t strlen(const char* s)
{
	register size_t len asm ("d0") = 0;
	__asm__ volatile (
		"move.l	%1, %%a0\n\t"
		"move.l	%%a0, %0\n\t"
		"1:\n\t"
		"tst.b	%%a0@+\n\t"
		"bne.s	1b\n\t"
		"sub.l	%%a0, %0\n\t"		// -(len + 1)
		"not.l	%0\n\t"				// !(-(len + 1)) = len
		: "=d" (len)
		: "g" (s)
		: "a0", "cc");
	return len;
}

void* memset(void *ptr, int value, size_t num)
{
	if (num > 0)
	{
	__asm__ volatile (
		"move.l	%0, %%a0\n\t"
		"move.w	%1, %%d1\n\t"
		"move.l	%2, %%d0\n\t"
		"1:\n\t"
		"move.b	%%d1, %%a0@+\n\t"
		"subq.l #1, %%d0\n\t"
		"bne.s	1b\n\t"
		:
		: "g" (ptr), "g" (value), "g" (num)
		: "a0", "d0", "d1", "cc", "memory");
	}
	return ptr;
}