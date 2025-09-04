/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

	.global Mfp_ActiveEdgeRegister
	.global CtrlC_enable

	.text

	.global InitMfpCom1
InitMfpCom1:
	.func InitMfpCom1
	move.l	4(a7), SerialExceptionCall + 2

	move.w	sr, -(a7)
	ori.w	#0x700, sr
	move.b	0xfffffa03.w, Mfp_ActiveEdgeRegister
	move.l	0x104.w, oMfpDcd + 2
	move.l	#MfpDcd, 0x104.w
	move.l	0x130.w, oSerialInput + 2
	move.l	#SerialInput, 0x130.w
	move.w	(a7)+, sr
	rts
	.endfunc

	.global ExitMfpCom1
ExitMfpCom1:
	.func ExitMfpCom1
	move.w	sr, -(a7)
	ori.w	#0x700, sr
	move.l	oMfpDcd + 2, 0x104.w
	move.l	oSerialInput + 2, 0x130.w
	move.w	(a7)+, sr
	rts
	.endfunc

/*
	Special for serial comm.
	If CTRL-C checking is enabled, and a CTRL-C (0x03) is detected, then pause the execution.
*/
SerialInput:
	tst.w	CtrlC_enable
	beq.s	oSerialInput
	btst	#7, 0xfffffa2b.w
	beq.s	o2SerialInput
	cmp.b	#3, 0xfffffa2f.w	| CTRL-C from gdb
	bne.s	o2SerialInput
	ori.w	#0x700, sr
	clr.w	CtrlC_enable	| Clear CtrlC_enable so we don't handle that while the server context is running. 
	move.b	#0xef, 0xfffffa0f.w	| enable irq again so serial comm works in server.
SerialExceptionCall:
	jmp		0x12345678
o2SerialInput:
	move.b	#0xef, 0xfffffa0f.w
	rte
oSerialInput:
	jmp 	0x12345678

/*
	Capture the mfp state, so we can easily detect DCD
*/
MfpDcd:
	move.b	0xfffffa03.w, Mfp_ActiveEdgeRegister
oMfpDcd:
	jmp 	0x12345678

