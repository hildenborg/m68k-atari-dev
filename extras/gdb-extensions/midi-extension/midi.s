/*
	Copyright (C) 2026 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

	.equ	COMM_ERR_DISCONNECTED,	-1
	.equ	COMM_ERR_KILLED,		-2
	.equ	COMM_ERR_NOT_READY,		-3

	.text

/*
	Return non 0 in d0 if we for some reason cannot run on this computer.
	The name should be checked, and in some cases the computer type.
*/
Midi_IsMyDevice:
	movem.l	a0-a1, -(a7)
	move.l	12(a7), a0
	lea		Midi_CookieName, a1
1:
	move.b	(a0)+, d0
	cmp.b	(a1)+, d0
	bne.s	2f
	tst.b	d0
	bne.s	1b
	moveq	#1, d0		| This was my device.
	bra.s	3f
2:
	moveq	#0, d0		| This was not my device.
3:
	movem.l	(a7)+, a0-a1
	rts


Midi_DeviceName:
	move.l	#Midi_Name, d0
	rts

/*
	Called in supervisor mode.
	4(a7) = comstring
	8(a7) = callback to enter debugger.
*/
Midi_Init:
	move.l	8(a7), MidiExceptionCall + 2

	move.w	sr, -(a7)
	ori.w	#0x700, sr
	move.l	0x118.w, Midi_receive_exit + 2
	move.l	#Midi_receive, 0x118.w
	move.w	(a7)+, sr
	moveq   #0, d0
	rts
	
/*
	Called in supervisor mode.
*/
Midi_Exit:
	move.w	sr, -(a7)
	ori.w	#0x700, sr
	move.l	Midi_receive_exit + 2, 0x118.w
	move.w	(a7)+, sr
	rts
	
Midi_TransmitByte:
	movem.l	d1-d2/a0-a3, -(a7)
	move.w	#4, -(a7)		| Should be 3, but a bug in Tos 1.0 swapped 3 and 4...
	move.w	#0x08, -(a7)
	trap	#13
	addq.l	#4, a7
	tst.w	d0
	bne.s	1f		| We can send data
	bsr		Midi_IsConnected
	tst.w	d0
	bne.s	2f
	moveq	#COMM_ERR_DISCONNECTED, d0
	bra.s	3f
2:
	moveq	#COMM_ERR_NOT_READY, d0
	bra.s	3f
1:
	move.w	30(a7), d0
	and.w	#0xff, d0
	move.w	d0, -(a7)
	move.w	#3, -(a7)
	move.w	#0x03, -(a7)
	trap	#13
	addq.l	#6, a7
	moveq	#0, d0
3:
	movem.l	(a7)+, d1-d2/a0-a3
	rts
	
Midi_ReceiveByte:
	movem.l	d1-d2/a0-a3, -(a7)
	move.w	#3, -(a7)
	move.w	#0x01, -(a7)
	trap	#13
	addq.l	#4, a7
	tst.w	d0
	bne.s	1f		| We can recieve data
	bsr		Midi_IsConnected
	tst.w	d0
	bne.s	2f
	moveq	#COMM_ERR_DISCONNECTED, d0
	bra.s	3f
2:
	moveq	#COMM_ERR_NOT_READY, d0
	bra.s	3f
1:
	move.w	#3, -(a7)
	move.w	#0x02, -(a7)
	trap	#13
	addq.l	#4, a7
	and.l	#0xff, d0
3:
	movem.l	(a7)+, d1-d2/a0-a3
	rts

Midi_IsConnected:
	/*
		No way to detect if we have a midi connection.
		So we just assume we have one.
	*/
	moveq	#1, d0
	rts
	
Midi_SetCtrlCFlag:
	move.b	7(a7), Midi_CtrlCFlag
	rts

/*
	This is the keybard and midi interrupt.
	Normally it should just jump to the original
	interrupt vector.

	However if Midi_CtrlCFlag is set, that means that the inferior is running and
	we want to be able to pause execution if a CTRL-C is sent from gdb.
	If we get a CTRL-C, then we want to jump to the callback we got in Midi_Init.
*/
Midi_receive:
	tst.b	Midi_CtrlCFlag
	jeq		Midi_receive_exit	| CTRL-C inactive
	move.b	0xfffffc04.w, Midi_StatusReg
	btst	#7, Midi_StatusReg
	jeq		Midi_receive_exit	| No IRQ
	btst	#0, Midi_StatusReg
	jeq		Midi_receive_exit	| No data
	move.b	0xfffffc06.w, Midi_Tmpdata
	cmp.b	#3, Midi_Tmpdata
	jne		Midi_receive_exit
	/*
		Got CTRL-C during inferior execution.
		As we don't continue with the TOS irq, 
		we need to make sure that the acia is in a good mood before contimnuing.
	*/
	move.b	0xfffffc04.w, Midi_StatusReg
	btst	#5, Midi_StatusReg
	jeq		1f					| No overrun
	move.b	0xfffffc06.w, Midi_Tmpdata	| Get data but ignore the error.
1:
	ori.w	#0x700, sr
	clr.b	Midi_CtrlCFlag	| Must turn off CTRL-C handling before entering debugger. 
	move.b	#0xbf, 0xfffffa11.w	| enable irq again so keyboard and midi works in server.
MidiExceptionCall:
	/*
		When/if we get here, the following things must be true:
		1. The stack pointer must be at the same position it was when entering this interrupt.
		2. IRQ's are off (sr | 0x700)
		3. The flag for testing CTRL-C should be turned off. 
	*/
	/*
		This jump will be modified to point to the callback arg in Midi_Init.
	*/
	jmp		0x12345678
Midi_receive_exit:
	/*
		This jump will be modified to point to the original interrupt vector.
	*/
	jmp		0x12345678

	.data
	.global GdbSrvComStruct
GdbSrvComStruct:
	.dc.l	1
	.dc.l	Midi_IsMyDevice
	.dc.l	Midi_DeviceName
	.dc.l	Midi_Init
	.dc.l	Midi_Exit
	.dc.l	Midi_TransmitByte
	.dc.l	Midi_ReceiveByte
	.dc.l	Midi_IsConnected
	.dc.l	Midi_SetCtrlCFlag

Midi_Name:
	.asciz	"Midi serial device.\r\n"
	.even

Midi_CookieName:
	.asciz	"SDBG"
	.even

	.bss
	.lcomm	Midi_CtrlCFlag, 1
	.lcomm	Midi_StatusReg, 1
	.lcomm	Midi_Tmpdata, 1
	.even
