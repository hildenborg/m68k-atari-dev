/*
 * The function that GCC 16.1 miscompiles at -O2 for m68k.
 *
 * Nothing here is undefined or Atari specific for the compiler: it is just a
 * copy from an array to a constant address. On Atari machines, that address
 * is the ST palette. Only 15 of the 16 registers are written, so even the
 * misplaced writes of the miscompiled code stay inside the palette.
 *
 * COPY_NAME is set by the makefile, to build this file with different options.
 */

#ifndef COPY_NAME
#define COPY_NAME copy
#endif

#define DEST ((volatile unsigned short *)0xffff8240UL)
#define COUNT 15

extern const unsigned short src[COUNT];

void COPY_NAME(void)
{
	int i;

	for (i = 0; i < COUNT; i++)
		DEST[i] = src[i];
}
