/*
 * Runs the same function (copy.c) compiled with and without the pass that
 * triggers the bug, and compares what ended up in memory.
 */

#include <stdio.h>
#include <tos.h>

#define PALETTE ((volatile unsigned short *)0xffff8240UL)
#define COUNT 15	/* registers written by copy.c */
#define TOTAL 16	/* registers in the palette */
#define MASK 0x0777	/* bits which exist on all machines */
#define MARKER 0x0707

/* Not static: copy.c must not know the values. */
const unsigned short src[COUNT] = {
	0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007, 0x010,
	0x020, 0x030, 0x040, 0x050, 0x060, 0x070, 0x100
};

void copy_bug(void);
void copy_ok(void);

static void run(void (*copy)(void), unsigned short *result)
{
	unsigned short saved[TOTAL];
	int i;

	for (i = 0; i < TOTAL; i++)
	{
		saved[i] = PALETTE[i];
		PALETTE[i] = MARKER;
	}
	copy();
	for (i = 0; i < TOTAL; i++)
	{
		result[i] = PALETTE[i] & MASK;
		PALETTE[i] = saved[i];
	}
}

static int report(const char *name, const unsigned short *result)
{
	int i, errors = 0;

	for (i = 0; i < TOTAL; i++)
	{
		if (result[i] != (i < COUNT ? src[i] : MARKER))
			errors++;
	}
	printf("%s: %s\n", name, errors ? "WRONG" : "correct");
	if (errors)
	{
		printf(" got: ");
		for (i = 0; i < TOTAL; i++)
			printf("%03x%s", result[i], i == 7 ? "\n      " : " ");
		printf("\n want:");
		for (i = 0; i < TOTAL; i++)
			printf(" %03x%s", i < COUNT ? src[i] : MARKER, i == 7 ? "\n     " : "");
		printf("\n");
	}
	return errors;
}

static unsigned short bug[TOTAL], ok[TOTAL];

/* Accessing the palette requires supervisor mode. */
static void run_all(void)
{
	run(copy_bug, bug);
	run(copy_ok, ok);
}

int main(void)
{
	int errors;

	Supexec((INT32_TOS_CB)run_all);

	printf("GCC %s\n", __VERSION__);
	errors = report("-O2", bug);
	errors += report("-O2 -fno-late-combine-instructions", ok);
	printf("%s\n", errors ? "BUG REPRODUCED" : "BUG NOT PRESENT");
	printf("Press a key\n");
	fflush(stdout);
	Cconin();
	return errors != 0;
}
