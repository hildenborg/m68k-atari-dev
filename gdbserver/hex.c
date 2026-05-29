/*
	Copyright (C) 2026 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#include "hex.h"

const char hex[] = "0123456789abcdef";

int HexToNibble(char c)
{
	return (c - (c < 'A' ? '0' : ((c < 'a' ? 'A' : 'a') - 10))) & 0xf;
}

unsigned char HexToByte(char* ptr)
{
	return (unsigned char)((HexToNibble(ptr[0]) << 4) | HexToNibble(ptr[1]));
}

// Endian aware
unsigned int HexToLong(char* ptr)
{
	unsigned int val = 0;
	for (int i = 0; i < 4; ++i)
	{
		val = (val << 8 ) | HexToByte(ptr);
		ptr += 2;
	}
	return val;
}

int HexConvertByteArray(char *hexArray)
{
	// To save memory, we just convert the array in the same buffer.
	char* hexptr = hexArray;
	char* byteArray = hexArray;
	while (*hexptr != 0)
	{
		*byteArray++ = (char)HexToByte(hexptr);
		hexptr += 2;
	}
	*byteArray = 0;
	return byteArray - hexArray;
}

// Unknown length.
int HexToVariable(char* ptr)
{
	unsigned int val = 0;
	while (*ptr != 0)
	{
		val = (val << 4 ) | (unsigned int)HexToNibble(*ptr++);
	}
	return val;
}

