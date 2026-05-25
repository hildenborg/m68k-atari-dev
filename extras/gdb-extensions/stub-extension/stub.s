/*
	Copyright (C) 2026 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/
/*
	This is a stub code for gdbserver communication extension.
	Only one extension can be loaded at any one time on the computer.
	If gdbserver finds an extension, then that will be the default
	communication method.
*/
	.equ	COMM_ERR_DISCONNECTED,	-1
	.equ	COMM_ERR_KILLED,		-2
	.equ	COMM_ERR_NOT_READY,		-3

	.text

/*
	C declaration: bool Stub_IsMyDevice(const char *comString);
	4(a7) = comString.
	Return result in d0.

	This function should verify that the comString is "SDBG" and 
	that the extension can run on this computer.
*/
Stub_IsMyDevice:
	movem.l	a0-a1, -(a7)
	move.l	12(a7), a0
	lea		Stub_CookieName, a1
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

/*
	C declaration: const char* Stub_DeviceName(void);
	Return result in d0.

	Return pointer to string that will be output to console if --debug option is set.
*/
Stub_DeviceName:
	move.l	#Stub_Name, d0
	rts

/*
	C declaration: int Stub_Init(const char *comString, _CommException CommException);
	4(a7) = comstring
	8(a7) = callback to enter debugger.
	Return 0 in d0 for success.

	This function inits and starts the communication.
	It is also resposible for setting up whatever is needed for detecting
	pause in execution (CTRL-C).

	If there is no way of intercepting the character receive through an interrupt,
	then pausing of execution is not possible.

	This function is run in supervisor mode.
*/
Stub_Init:
	move.l	8(a7), StubExceptionCall + 2

	move.w	sr, -(a7)
	ori.w	#0x700, sr
	/*
		Pseudocode:
			StubIRQ = irq vector for receive interrupt
		move.l	StubIRQ, Stub_receive_exit + 2
		move.l	#Stub_receive, StubIRQ
	*/
	move.w	(a7)+, sr
	moveq	#0, d0
	rts
	
/*
	C declaration: void Stub_Exit(void);

	This function turns off and cleanly exits anything that Stub_Init have done.

	This function is run in supervisor mode.
*/
Stub_Exit:
	move.w	sr, -(a7)
	ori.w	#0x700, sr
	/*
		Pseudocode:
			StubIRQ = irq vector for receive interrupt
		move.l	Stub_receive_exit + 2, StubIRQ
	*/
	move.w	(a7)+, sr
	rts

/*
	C declaration: int Stub_TransmitByte(unsigned char byte);
	7(a7) = byte
	Return d0 =
		0 for byte sent.
		COMM_ERR_DISCONNECTED for connection lost.
		COMM_ERR_NOT_READY transmit buffer is full.

	This function is non-blocking and either transmits a byte or returns with error.
*/
Stub_TransmitByte:
	rts
	
/*
	C declaration: int Stub_ReceiveByte(void);
	Return d0 =
		0 to 255 for received byte.
		COMM_ERR_DISCONNECTED for connection lost.
		COMM_ERR_NOT_READY transmit buffer is full.

	This function is non-blocking and either receives a byte or returns with error.
*/
Stub_ReceiveByte:
	rts

/*
	C declaration: bool Stub_IsConnected(void);
	Return d0 =
		1 for connected to remote computer.
		0 for disconnected from remote computer.
	
	If detection is not possible, just return 1.
*/
Stub_IsConnected:
	moveq	#1, d0
	rts

/*
	C declaration: void Stub_EnableCtrlC(bool enable);
	7(a7) = enable

	This function enables/disables the CTRL-C pause function.
	Normally the flag is just stored and then checked in the receiver interrupt.
*/	
Stub_SetCtrlCFlag:
	move.b	7(a7), Stub_CtrlCFlag
	rts

/*
	This is the receiver interrupt and varies a lot between communication protocols.
	So the code below is the best general example possible.
	For more information, look at the midi-extension example, or the gdbserver MFP/SCC code.
*/
Stub_receive:
	tst.b	Stub_CtrlCFlag
	jeq		Stub_receive_exit	| CTRL-C inactive
	/*
		We can only get here if the inferior is running, so the gdbserver is not
		using any of the received data at this moment.
		So we fetch the incomming byte in RECEIVED_BYTE and check for CTRL-C (0x03)
		and in that case pause execution.
		Anything else than CTRL-C can just be discarded.
	*/
	cmp.b	#3, RECEIVED_BYTE
	jne		Stub_receive_exit
	/*
		Got CTRL-C during inferior execution, pause into gdbserver.
		This is done by:
			1. Turn off all irq's.
			2. Disable CTRL-C.
			3. Make sure that *all* registers except for SR is the exact same as when
				entering this interrupt.
			4. Jump to the callback we got in Stub_Init.
	*/
	ori.w	#0x700, sr
	clr.b	Stub_CtrlCFlag	| Must turn off CTRL-C handling before entering debugger. 
StubExceptionCall:
	/*
		This jump is modified by Stub_Init to point at debugger callback.
	*/
	jmp		0x12345678
Stub_receive_exit:
	/*
		This jump is modified by Stub_Init to point at original interrupt address.
	*/
	jmp		0x12345678

	.data
	.global GdbSrvComStruct
GdbSrvComStruct:
	.dc.l	1
	.dc.l	Stub_IsMyDevice
	.dc.l	Stub_DeviceName
	.dc.l	Stub_Init
	.dc.l	Stub_Exit
	.dc.l	Stub_TransmitByte
	.dc.l	Stub_ReceiveByte
	.dc.l	Stub_IsConnected
	.dc.l	Stub_SetCtrlCFlag

Stub_Name:
	.asciz	"Stub serial device.\r\n"
	.even

Stub_CookieName:
	.asciz	"SDBG"
	.even

	.bss
	.lcomm	Stub_CtrlCFlag, 1
	.even
