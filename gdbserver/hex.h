/*
	Copyright (C) 2026 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#ifndef HEX_DEFINED
#define HEX_DEFINED

extern const char hex[];

#define NibbleToHex(nibb) hex[(nibb) & 0xf]

int HexToNibble(char c);
unsigned char HexToByte(char* ptr);
unsigned int HexToLong(char* ptr);
int HexConvertByteArray(char *hexArray);
int HexToVariable(char* ptr);

#endif // HEX_DEFINED
