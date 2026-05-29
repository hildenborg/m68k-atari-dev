/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#ifndef CLIB_DEFINED
#define CLIB_DEFINED

#include <stddef.h>

size_t strlen(const char* s);

void* memset(void *ptr, int value, size_t num);

short StringCompare(const char* str_a, const char* str_b);

char* StrCopy(const char* source, char* dest);

#endif // CLIB_DEFINED
