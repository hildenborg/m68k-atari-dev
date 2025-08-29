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
	size_t len = 0;
	__asm__ volatile (
		"move.l	%1, %0\n\t"
		"1:\n\t"
		"tst.b	%1@+\n\t"
		"bne.s	1b\n\t"
		"sub.l	%1, %0\n\t"		// -(len + 1)
		"not.l	%0\n\t"			// !(-(len + 1)) = len
		: "=d" (len)
		: "a" (s)
		:);
	return len;
}

void* memset(void *ptr, int value, size_t num)
{
	__asm__ volatile (
		"bra.s	2f\n\t"
		"1:\n\t"
		"move.b	%1, %0@+\n\t"
		"2:\n\t"
		"subq.l #1, %2\n\t"
		"bpl.s	1b\n\t"
		:
		: "a" (ptr), "r" (value), "r" (num)
		:);
	return ptr;
}