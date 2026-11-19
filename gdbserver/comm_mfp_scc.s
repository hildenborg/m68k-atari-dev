/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

	.global Mfp_ActiveEdgeRegister
	.global CtrlC_enable
	.global sccDelayCount

	.text

	.global InitMfpAux
InitMfpAux:
	.func InitMfpAux
	move.l	4(a7), MfpSerialExceptionCall + 2

	move.w	sr, -(a7)
	ori.w	#0x700, sr
	move.b	0xfffffa03.w, Mfp_ActiveEdgeRegister
	move.l	0x104.w, oMfpDcd + 2
	move.l	#MfpDcd, 0x104.w
	move.l	0x130.w, oMfpSerialInput + 2
	move.l	#MfpSerialInput, 0x130.w
	move.w	(a7)+, sr
	rts
	.endfunc

	.global ExitMfpAux
ExitMfpAux:
	.func ExitMfpAux
	move.w	sr, -(a7)
	ori.w	#0x700, sr
	move.l	oMfpDcd + 2, 0x104.w
	move.l	oMfpSerialInput + 2, 0x130.w
	move.w	(a7)+, sr
	rts
	.endfunc

MfpSerialInput:
	tst.w	CtrlC_enable
	beq.s	oMfpSerialInput
	btst	#7, 0xfffffa2b.w
	beq.s	o2MfpSerialInput
	cmp.b	#3, 0xfffffa2f.w	| CTRL-C from gdb
	bne.s	o2MfpSerialInput
	ori.w	#0x700, sr
	clr.w	CtrlC_enable	| Clear CtrlC_enable so we don't handle that while the server context is running. 
	move.b	#0xef, 0xfffffa0f.w	| enable irq again so serial comm works in server.
MfpSerialExceptionCall:
	jmp		0x12345678
o2MfpSerialInput:
	move.b	#0xef, 0xfffffa0f.w
	rte
oMfpSerialInput:
	jmp 	0x12345678

MfpDcd:
	move.b	0xfffffa03.w, Mfp_ActiveEdgeRegister
oMfpDcd:
	jmp 	0x12345678

/*
	The SCC is driven by TOS through the BIOS, like the MFP. Only the channel B receive
	interrupt is hooked, to catch CTRL-C from gdb while the inferior runs.
	TOS (and EmuTOS) set the SCC vector base to 0x180, so channel B "receive character available"
	is at 0x190 and "special receive condition" at 0x198. A received byte can come through either,
	so both are hooked.
*/
	.equ	SccCtrlB, 0xffff8c85
	.equ	SccDataB, 0xffff8c87
	.equ	SccRxBVector, 0x190
	.equ	SccSpecialBVector, 0x198

	.global InitSccAux
InitSccAux:
	.func InitSccAux
	move.l	4(a7), SccSerialExceptionCall + 2
	jbsr	SccCalcDelay
	move.w	sr, -(a7)
	ori.w	#0x700, sr
	move.l	SccRxBVector.w, oSccSerialInput + 2
	move.l	#SccSerialInput, SccRxBVector.w
	move.l	SccSpecialBVector.w, oSccSpecialInput + 2
	move.l	#SccSpecialInput, SccSpecialBVector.w
	move.w	(a7)+, sr
	moveq	#0, d0
	rts
	.endfunc

	.global ExitSccAux
ExitSccAux:
	.func ExitSccAux
	move.w	sr, -(a7)
	ori.w	#0x700, sr
	move.l	oSccSerialInput + 2, SccRxBVector.w
	move.l	oSccSpecialInput + 2, SccSpecialBVector.w
	move.w	(a7)+, sr
	rts
	.endfunc

/*
	Like MfpSerialInput: while CTRL-C is enabled the inferior runs, and gdb only sends CTRL-C.
	Then the byte is read here and not passed on to TOS. Otherwise TOS handles the interrupt.
*/
SccSpecialInput:
	tst.w	CtrlC_enable
	beq.s	oSccSpecialInput
	bra.s	SccCheckCtrlC
oSccSpecialInput:
	jmp 	0x12345678

SccSerialInput:
	tst.w	CtrlC_enable
	beq.s	oSccSerialInput
SccCheckCtrlC:
	movem.l	d0-d1, -(a7)
	move.b	SccCtrlB.w, d1		| RR0
	jbsr	SccDelay
	btst	#0, d1
	beq.s	1f					| No data
	move.b	SccDataB.w, d1
	jbsr	SccDelay
	cmp.b	#3, d1				| CTRL-C from gdb
	bne.s	1f
	ori.w	#0x700, sr
	clr.w	CtrlC_enable	| Clear CtrlC_enable so we don't handle that while the server context is running.
	move.b	#0x38, SccCtrlB.w	| Reset highest IUS, so serial comm works in server.
	jbsr	SccDelay
	movem.l	(a7)+, d0-d1
SccSerialExceptionCall:
	jmp		0x12345678
1:
	move.b	#0x38, SccCtrlB.w	| Reset highest IUS
	jbsr	SccDelay
	movem.l	(a7)+, d0-d1
	rte
oSccSerialInput:
	jmp 	0x12345678

/*
	Count how many loops we make during a 200hz tick.
	This is used to calc the four cycle recovery time for the SCC.
*/
SccCalcDelay:
	move.l	d1, -(a7)
	moveq	#0, d1
	move.l	0x4ba.w, d0
1:
	cmp.l	0x4ba.w, d0
	beq.s	1b
	move.l	0x4ba.w, d0
1:
	addq.l	#1, d1
	cmp.l	0x4ba.w, d0
	beq.s	1b
	divu	#10000, d1		| will give us loop count for 0.5us
	and.l	#0xffff, d1
	bne.s	2f
	| don't need a delay loop.
	move.w	rtsinst, SccDelay
2:
	move.l	d1, sccDelayCount
	move.l	(a7)+, d1
rtsinst:
	rts

/*
	This will delay at a minimum 0.5us.
	The slower the computer, the more inexact it will be.
*/
SccDelay:
	move.l	sccDelayCount, d0
1:
	cmp.l	0x4ba.w, d0		| Only used to make the delay loop use same amount of cycles as calc loop. 
	subq.l	#1, d0
	bne.s	1b
	rts

	.bss
	.lcomm	sccDelayCount, 4
	.even
