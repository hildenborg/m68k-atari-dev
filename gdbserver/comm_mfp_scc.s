/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

	.global Mfp_ActiveEdgeRegister
	.global CtrlC_enable

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
	move.w	sr, -(a7)
	ori.w	#0x700, sr

/*
	move.l	0x188.w, oSccStatus + 2
	move.l	#SccStatus, 0x188.w
	move.l	0x190.w, oSccSerialInput + 2
	move.l	#SccSerialInput, 0x190.w
	move.b	#0, 0xffff8c85.w
	move.b	0xffff8c85.w, Scc_StatusRegister
*/
	move.w	(a7)+, sr
	moveq	#0, d0
	rts
	.endfunc

	.global ExitSccAux
ExitSccAux:
	.func ExitSccAux
	move.w	sr, -(a7)
	ori.w	#0x700, sr

/*
	move.l	oSccStatus + 2, 0x188.w
	move.l	oSccSerialInput + 2, 0x190.w
*/
	move.w	(a7)+, sr
	rts
	.endfunc

SccStatus:
	move.b	#0, 0xffff8c85.w
	move.b	0xffff8c85.w, Scc_StatusRegister
oSccStatus:
	jmp 	0x12345678

SccSerialInput:
	tst.w	CtrlC_enable
	jeq		oSccSerialInput
	move.b	#0, 0xffff8c85.w
	btst	#0, 0xffff8c85.w
	jeq		o2SccSerialInput
	cmp.b	#3, 0xffff8c87.w	| CTRL-C from gdb
	jne		o2SccSerialInput
	ori.w	#0x700, sr
	clr.w	CtrlC_enable	| Clear CtrlC_enable so we don't handle that while the server context is running. 
	move.b	#0, 0xffff8c85.w
	move.b	#0x30, 0xffff8c85.w		|reset ext/status int
	move.b	#0, 0xffff8c85.w
	move.b	#0x38, 0xffff8c85.w		|reset highest IUS
SccSerialExceptionCall:
	jmp		0x12345678
o2SccSerialInput:
	move.b	#0, 0xffff8c85.w
	move.b	#0x30, 0xffff8c85.w		|reset ext/status int
	move.b	#0, 0xffff8c85.w
	move.b	#0x38, 0xffff8c85.w		|reset highest IUS
	rte
oSccSerialInput:
	jmp 	0x12345678

