/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

	.equ	DEFAULT_STACK, 2048

	.global _BasePage
	.global __BSS_SEGMENT_END

	.section	".init"
	.global	_start
	.type	_start,#function
_start:
	move.l	4(a7),a0
	move.l	a0, _BasePage

	/*
		Set up memory to use just a minimum of stack and heap.
	*/
    lea     __BSS_SEGMENT_END + DEFAULT_STACK, a7
    move.l	a7, a1
	sub.l	(a0), a1

	move.l	a1, -(a7)
	move.l	a0, -(a7)
	clr.w	-(a7)
	move.w	#0x4a, -(a7)	| Mshrink()
	trap	#1
	lea		12(a7), a7
	tst.l	d0
	jmi		exit_program

/*
	Need to implement "The Atari Extended Argument Specification".
	Also, there is no guarantee that the command line is zero terminated.
*/
    | this occurs after crtbegin.o have done all global constructors etc.

	move.l	a7, a6
	moveq	#0, d0
	moveq	#0, d2
	movea.l	_BasePage, a0
	lea		128(a0), a0		| add offset to the cmdline
	lea		_cmdline, a1
	move.b	(a0)+, d2		| d2 contains number of bytes in command line (max 127)
	beq.s	3f
	clr.b	(a1, d2.w)		| end our decoded args with a zero.
	bra.s	2f
1:
	move.b	(a0, d2.w), d1
	cmp.b	#' ', d1
	bne.s	4f
	moveq	#0, d1
	lea		1(a1, d2.w), a2
	tst.b	(a2)
	beq.s	4f
	move.l	a2, -(a7)
	addq.w	#1, d0
4:
	move.b	d1, (a1, d2.w)
2:
	subq.w	#1, d2
	bcc.s	1b
	move.l	a1, -(a7)
	addq.w	#1, d0
3:
	pea		_procname		| first argument is always the proc name. That we do not know...
	addq.w	#1, d0
	move.l	a7, a5
	move.l	a6, -(a7)		| To know where to move it back again.

	move.l	a5, -(a7)		| argv
	move.l	d0, -(a7)		| argc
	jsr		main
	move.l	8(a7), a7		| move it back.

exit_program:
	move.w	d0, -(a7)
	move.w	#0x4c, -(a7)
	trap	#1

	.data
_procname:
	.asciz	"gdbsrv.ttp"
	.even

	.bss
	.lcomm 	_BasePage, 4
	.lcomm	_cmdline, 128
	.even

