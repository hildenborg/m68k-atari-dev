/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

	.global Mfp_ActiveEdgeRegister
	.global CtrlC_enable
	.global sccTmpData
	.global sccDelayCount

	.equ	SccBufferLength, 0x100	| factor of two numbers only
	.equ	SccBufferMask, SccBufferLength - 1

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

	.global InitSccAux
InitSccAux:
	.func InitSccAux
	move.l	4(a7), SccSerialExceptionCall + 2
	jbsr	SccCalcDelay
	movem.l	d1-d2/a0, -(a7)
	move.w	sr, -(a7)
	ori.w	#0x700, sr

	lea		sccVectors, a0
	move.l	0x180.w, (a0)+
	move.l	0x188.w, (a0)+
	move.l	0x190.w, (a0)+
	move.l	0x198.w, (a0)+

	move.l	#SccSerialTX, 0x180.w
	move.l	#SccStatus, 0x188.w
	move.l	#SccSerialRX, 0x190.w
	move.l	#SccSerialRX, 0x198.w
/*
	| Make sure all TX data is finished sending.
	| Wait for at most ten milliseconds.
	move.w	#10000, d2
1:
	move.b	#1, 0xffff8c85.w
	jbsr	SccDelay
	move.b	0xffff8c85.w, d1
	jbsr	SccDelay
	btst	#0, d1
	jne		2f
	dbra	d2, 1b
2:

	| Clear all errors and fifo
	move.b	#0x30, 0xffff8c85.w
	jbsr	SccDelay
	move.b	0xffff8c85.w, d0
	jbsr	SccDelay
	moveq	#2, d1
1:
	move.b	0xffff8c87.w, d0
	jbsr	SccDelay
	dbra	d1, 1b
	move.b	0xffff8c85.w, d0
	jbsr	SccDelay
	move.b	#0x30, 0xffff8c85.w
	jbsr	SccDelay
*/
	| Load registers to set 9600 8N1 and interrupts enabled
	lea		SccInit, a0
	move.b	(a0)+, 0xffff8c85.w
	jbsr	SccDelay
	move.b	(a0)+, 0xffff8c85.w
	jbsr	SccDelay
	jbsr	SccDelay
1:
	move.b	(a0)+, 0xffff8c85.w
	jbsr	SccDelay
	cmpa.l	#SccInitEnd, a0
	jne		1b
	move.b	0xffff8c85.w, Scc_StatusRegister
	jbsr	SccDelay

	move.w	(a7)+, sr
	movem.l	(a7)+, d1-d2/a0
	moveq	#0, d0
	rts
	.endfunc

	.global ExitSccAux
ExitSccAux:
	.func ExitSccAux
	move.l	a0, -(a7)
	move.w	sr, -(a7)
	ori.w	#0x700, sr

	lea		sccVectors, a0
	move.l	(a0)+, 0x180.w
	move.l	(a0)+, 0x188.w
	move.l	(a0)+, 0x190.w
	move.l	(a0)+, 0x198.w

	move.w	(a7)+, sr
	move.l	(a7)+, a0

	rts
	.endfunc

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

SccStatus:
	move.l	d0, -(a7)
	move.b	0xffff8c85.w, Scc_StatusRegister
	jbsr	SccDelay
	moveq	#0x10, d0
	jbsr	SccClearIrq
	move.l	(a7)+, d0
	rte

SccSerialRX:
	movem.l d0-d1/a0, -(a7)

	move.b	0xffff8c85.w, Scc_StatusRegister
	jbsr	SccDelay
	btst	#0, Scc_StatusRegister
	jeq		SccSerialRXExit		| No data
	move.b	0xffff8c87.w, d1
	jbsr	SccDelay
	tst.w	CtrlC_enable
	jeq		SccSerialReceive	| This is a normal receive
	cmp.b	#3, d1
	jne		SccSerialRXExit		| No CTRL-C, discard the data and continue
	| Break the execution of the inferior.
	ori.w	#0x700, sr
	clr.w	CtrlC_enable	| Clear CtrlC_enable so we don't handle that while the server context is running. 
	moveq	#0x30, d0
	jbsr	SccClearIrq
	movem.l (a7)+, d0-d1/a0
SccSerialExceptionCall:
	jmp		0x12345678
SccSerialReceive:
	| store d1 in serial input fifo
	move.w	sccInputCount, d0
	cmp.w	#SccBufferLength, d0
	jeq		SccSerialRXExit		| Fifo overflow, data lost (should not happen if RTS is handled properly).
	add.w	sccInputPos, d0
	and.w	#SccBufferMask, d0
	lea		sccInputBuffer, a0
	move.b	d1, (a0, d0.w)
	add.w	#1, sccInputCount
	move.w	sccInputCount, d0
	cmp.w	#SccBufferLength, d0
	jne		SccSerialRXExit
/*
	The buffer is full. Disable RTS.
*/
	move.b	#0x05, 0xffff8c85.w		| Select write register 5
	jbsr	SccDelay
	move.b	#0xe8, 0xffff8c85.w		| Disable RTS
	jbsr	SccDelay

SccSerialRXExit:
	moveq	#0x30, d0
	jbsr	SccClearIrq
	movem.l (a7)+, d0-d1/a0
	rte

SccSerialTX:
	movem.l d0/a0, -(a7)
	moveq	#0x28, d0
	jbsr	SccClearIrq

	move.b	0xffff8c85.w, Scc_StatusRegister
	jbsr	SccDelay
	btst	#2, Scc_StatusRegister
	jeq		SccSerialTXExit		| SCC Buffer not empty
	move.w	sccOutputCount, d0
	jeq		SccSerialTXExit		| No data to send

	move.w	sccOutputPos, d0
	lea		sccOutputBuffer, a0
	move.b	(a0, d0.w), 0xffff8c87.w
	addq.w	#1, d0
	and.w	#SccBufferMask, d0
	move.w	d0, sccOutputPos
	sub.w	#1, sccOutputCount
	jbsr	SccDelay
SccSerialTXExit:
	movem.l (a7)+, d0/a0
	rte

SccClearIrq:
	move.b	d0, 0xffff8c85.w		|reset ext/status int
	jbsr	SccDelay
	move.b	#0x38, 0xffff8c85.w		|reset highest IUS
	jbsr	SccDelay
	rts

	.global SccBcostat
SccBcostat:
	.func SccBcostat
	move.l	#SccBufferLength, d0
	sub.w	sccOutputCount, d0
	rts
	.endfunc

	.global SccBconout
SccBconout:
	.func SccBconout
	move.l	d1, -(a7)
	move.w	sr, -(a7)
	ori.w	#0x700, sr
	move.b	0xffff8c85.w, Scc_StatusRegister
	jbsr	SccDelay
	move.w	sccTmpData, d0
	move.w	sccOutputCount, d1
	jne		1f
	btst	#2, Scc_StatusRegister
	jeq		1f
	| Send immediately
	move.b	d0, 0xffff8c87.w
	jbsr	SccDelay
	jbra	2f
1:
	| Put in buffer.
	move.w	(a7), sr
	| Wait for space in buffer
3:
	move.w	sccOutputCount, d1
	cmp.w	#SccBufferLength, d1
	jeq		3b
	ori.w	#0x700, sr
	move.w	sccOutputCount, d1 | read again after turning off irqs
	add.w	sccOutputPos, d1
	and.w	#SccBufferMask, d1
	move.l	a0, -(a7)
	lea		sccOutputBuffer, a0
	move.b	d0, (a0, d1.w)
	add.w	#1, sccOutputCount
	move.l	(a7)+, a0
2:
	move.w	(a7)+, sr
	move.l	(a7)+, d1
	rts
	.endfunc

	.global SccBconin
SccBconin:
	.func SccBconin
1:
	move.w	sccInputCount, d0
	jeq		1b		| Wait for data
	move.w	sr, -(a7)
	ori.w	#0x700, sr
	move.w	sccInputCount, d0	| Read again after turning off irqs

	move.l	a0, -(a7)
	move.l	d1, -(a7)
	move.w	sccInputPos, d0
	lea		sccInputBuffer, a0
	moveq	#0, d1
	move.b	(a0, d0.w), d1
	move.w	d1, sccTmpData
	addq.w	#1, d0
	and.w	#SccBufferMask, d0
	move.w	d0, sccInputPos
	move.w	sccInputCount, d0
	sub.w	#1, sccInputCount
	cmp.w	#SccBufferLength, d0
	jne		1f
/*
	The buffer is no longer full. We can enable RTS.
*/
	move.b	#0x05, 0xffff8c85.w		| Select write register 5
	jbsr	SccDelay
	move.b	#0xea, 0xffff8c85.w		| Enable RTS
	jbsr	SccDelay
1:
	move.l	(a7)+, d1
	move.l	(a7)+, a0
	move.w	(a7)+, sr
	rts
	.endfunc

	.global SccBconstat
SccBconstat:
	.func SccBconstat
	moveq	#0, d0
	move.w	sccInputCount, d0
	rts
	.endfunc

	.data
SccInit:
	.dc.b 0x09, 0x41	| Reset channel b and turn off interrupts
	.dc.b 0x04, 0x44	| x16 clock mode, 1 stop bit asynchronous mode.
	.dc.b 0x03, 0xc0	| Disable RX, 8 bit
	.dc.b 0x05, 0xe2	| Disable TX, DTR and RTS enabled, 8 bit

	.dc.b 0x02, 0x60	| Vector register

	.dc.b 0x0e, 0x02	| Disable BRG, set BRG source
	.dc.b 0x0b, 0x50	| Clock mode
	.dc.b 0x0c, 0x18	| Lower divisor
	.dc.b 0x0d, 0x00	| Upper divisor
	.dc.b 0x0e, 0x03	| Enable BRG

	.dc.b 0x0f, 0x01	| Select prime D7
	.dc.b 0x07, 0x00	| Clear prime D7
	.dc.b 0x0f, 0x08	| Select non prime D7 and turn on DCD interrupts
	.dc.b 0x01, 0x13	| Enable TX, RX and EXT interrupts.

	.dc.b 0x03, 0xc1	| Enable RX, auto enable CTS, 8 bit
	.dc.b 0x05, 0xea	| Enable TX, DTR and RTS enabled, 8 bit

	.dc.b 0x00, 0x10	| Reset external status interrupt
	.dc.b 0x00, 0x10	| Reset external status interrupt
	.dc.b 0x09, 0x09	| Master interrupt enable
SccInitEnd:

	.bss
	.lcomm	sccVectors, 4*4
	.lcomm	sccDelayCount, 4
	.lcomm	sccInputPos, 2
	.lcomm	sccInputCount, 2
	.lcomm	sccOutputPos, 2
	.lcomm	sccOutputCount, 2
	.lcomm	sccInputBuffer, SccBufferLength
	.lcomm	sccOutputBuffer, SccBufferLength
	.even
