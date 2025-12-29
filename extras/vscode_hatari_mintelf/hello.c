/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#include <stdio.h>
#include <string.h>

// To test data segment
const char *hello = "Hello world!\n";

// To test bss segment
static char buffer[100];

void print_hello(void)
{
	printf (buffer);
}

int main (int argc, char **argv)
{
	strcpy(buffer, hello);
	print_hello();
	return 0;
}

