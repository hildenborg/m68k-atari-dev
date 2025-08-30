/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#ifndef CLIB_DEFINED
#define CLIB_DEFINED

#include <stddef.h>

size_t strlen(const char* s);

void* memset(void *ptr, int value, size_t num);

#endif // CLIB_DEFINED
