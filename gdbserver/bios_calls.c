/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#include "bios_calls.h"

// Store stack pointer in a3, that by Atari documentation will be left untouched by the trap call.
#define TRAP_BEGIN "move.l		%%a7, %%a3\n\t"
// Make trap call and then restore the stack pointer from the stored value in a3
#define TRAP_FUNC(num, func) "move.w		#" #func ", %%a7@-\n\ttrap		#" #num "\n\tmove.l		%%a3, %%a7\n\t"
// Registers d1,d2 and a0, a1, a2 may be affected by trap #1 calls. Register a3 is used to store/restore a7
#define CLOBBER_REG "d1", "d2", "a0", "a1", "a2", "a3"

unsigned int Dsetdrv(unsigned short bios_drive)
{
	register unsigned int bios_mounted_drives asm ("d0") = 0;
	__asm__ volatile (
		TRAP_BEGIN
		"move.w		%1, %%a7@-\n\t"
		TRAP_FUNC(1, 0xe)
		: "=r" (bios_mounted_drives) 
		: "r" (bios_drive)
		: CLOBBER_REG);
	return bios_mounted_drives;
}

int Dsetpath(const char* bios_path)
{
	register int result asm ("d0") = -1;
	__asm__ volatile (
		TRAP_BEGIN
		"move.l		%1, %%a7@-\n\t"
		TRAP_FUNC(1, 0x3b)
		: "=r" (result) 
		: "r" (bios_path)
		: CLOBBER_REG);
	return result;
}

int Mfree(void* start_addr)
{
	register int result asm ("d0") = -1;
	__asm__ volatile (
		TRAP_BEGIN
		"move.l		%1, %%a7@-\n\t"
		TRAP_FUNC(1, 0x49)
		: "=r" (result) 
		: "r" (start_addr)
		: CLOBBER_REG);
	return result;
}

int Pexec(unsigned short mode, const char* file_name, const char* cmdline, const char* envstring)
{
	register int result asm ("d0") = -1;
	__asm__ volatile (
		TRAP_BEGIN
		"move.l		%4, %%a7@-\n\t"
		"move.l		%3, %%a7@-\n\t"
		"move.l		%2, %%a7@-\n\t"
		"move.w		%1, %%a7@-\n\t"
		TRAP_FUNC(1, 0x4b)
		: "=r" (result) 
		: "r" (mode), "r" (file_name), "r" (cmdline), "r" (envstring)
		: CLOBBER_REG);
	return result;
}

void __attribute__ ((noreturn)) Pterm(unsigned short retcode)
{
	__asm__ volatile (
		TRAP_BEGIN
		"move.w		%0, %%a7@-\n\t"
		TRAP_FUNC(1, 0x4c)
		: 
		: "r" (retcode)
		: CLOBBER_REG, "d0");
	__builtin_unreachable();
}

int Bconstat(unsigned short dev)
{
	register int result asm ("d0") = -1;
	__asm__ volatile (
		TRAP_BEGIN
		"move.w		%1, %%a7@-\n\t"
		TRAP_FUNC(13, 0x01)
		: "=r" (result) 
		: "r" (dev)
		: CLOBBER_REG);
	return result;
}

unsigned int Bconin(unsigned short dev)
{
	register unsigned int result asm ("d0") = 0;
	__asm__ volatile (
		TRAP_BEGIN
		"move.w		%1, %%a7@-\n\t"
		TRAP_FUNC(13, 0x02)
		: "=r" (result) 
		: "r" (dev)
		: CLOBBER_REG);
	return result;
}

unsigned int Bconout(unsigned short dev, unsigned short ch)
{
	register unsigned int result asm ("d0") = 0;
	__asm__ volatile (
		TRAP_BEGIN
		"move.w		%2, %%a7@-\n\t"
		"move.w		%1, %%a7@-\n\t"
		TRAP_FUNC(13, 0x03)
		: "=r" (result) 
		: "r" (dev), "r" (ch)
		: CLOBBER_REG);
	return result;
}

int Bcostat(unsigned short dev)
{
	register int result asm ("d0") = -1;
	__asm__ volatile (
		TRAP_BEGIN
		"move.w		%1, %%a7@-\n\t"
		TRAP_FUNC(13, 0x08)
		: "=r" (result) 
		: "r" (dev)
		: CLOBBER_REG);
	return result;
}


int Rsconf(unsigned short speed, unsigned short flow, unsigned short ucr, unsigned short rsr, unsigned short tsr, unsigned short scr)
{
	register int result asm ("d0") = -1;
	__asm__ volatile (
		TRAP_BEGIN
		"move.w		%6, %%a7@-\n\t"
		"move.w		%5, %%a7@-\n\t"
		"move.w		%4, %%a7@-\n\t"
		"move.w		%3, %%a7@-\n\t"
		"move.w		%2, %%a7@-\n\t"
		"move.w		%1, %%a7@-\n\t"
		TRAP_FUNC(14, 0x0f)
		: "=r" (result) 
		: "r" (speed), "r" (flow), "r" (ucr), "r" (rsr), "r" (tsr), "r" (scr)
		: CLOBBER_REG);
	return result;
}

void Offgibit(unsigned short mask)
{
	__asm__ volatile (
		TRAP_BEGIN
		"move.w		%0, %%a7@-\n\t"
		TRAP_FUNC(14, 0x1d)
		:
		: "r" (mask)
		: CLOBBER_REG, "d0");
}

void Ongibit(unsigned short mask)
{
	__asm__ volatile (
		TRAP_BEGIN
		"move.w		%0, %%a7@-\n\t"
		TRAP_FUNC(14, 0x1e)
		:
		: "r" (mask)
		: CLOBBER_REG, "d0");
}

int Supexec(int (*callback)(void))
{
	register int callback_return asm ("d0") = -1;
	__asm__ volatile (
		TRAP_BEGIN
		"move.l		%1, %%a7@-\n\t"
		TRAP_FUNC(14, 0x26)
		: "=r" (callback_return) 
		: "r" (callback)
		: CLOBBER_REG);
	return callback_return;
}
