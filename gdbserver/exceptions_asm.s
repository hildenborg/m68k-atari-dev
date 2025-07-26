/*
	m68k compatible.
	Current Serial implementation is specific to detecting CTRL-C on Stari ST/STE/TT.
*/
	.global registers
	.global Exception
	.global CtrlC_enable
	.global Mfp_ActiveEdgeRegister
	.global Cookie_CPU
	.global Cookie_VDO
	.global Cookie_FPU
	.global Cookie_MCH
	.global StoreVectors
	.global LoadVectors
	.global StoreMemoryRegisters
	.global LoadMemoryRegisters

	.macro HookVector name
	move.l	n\name, o\name + 2
	move.l	#\name, n\name
	.endm

	.macro UnHookVector name
	move.l	o\name + 2, n\name
	.endm

	.macro Hook name, num
	.equ	n\name,	\num * 4
\name:
	ori.w	#0x700, sr
	move.l	a7, exception_a7
	pea		\num
	jsr		SaveState
	move.w	sr, d0
	andi.w	#0xf8ff, d0
	or.w	srvIrqLevel, d0
	move.w	d0, sr
	jsr		Exception
	ori.w	#0x700, sr
	jsr		RestoreState
	move.l	exception_a7, a7
o\name:
	jmp 	0x12345678
	.endm

/*
	Like Hook, but we do not continue with the original vector.
*/
	.macro Hijack name, num
	.equ	n\name,	\num * 4
\name:
	ori.w	#0x700, sr
	move.l	a7, exception_a7
	pea		\num
	jsr		SaveState
	move.w	sr, d0
	andi.w	#0xf8ff, d0
	or.w	srvIrqLevel, d0
	move.w	d0, sr
	jsr		Exception
	ori.w	#0x700, sr
	jsr		RestoreState
	move.l	exception_a7, a7
	rte
o\name:
	jmp 	0x12345678
	.endm

/*
	Special for serial comm.
	If CTRL-C checking is enabled, and a CTRL-C (0x03) is detected, then pause the execution.
*/
	.macro S_Hook name, num
	.equ	n\name,	\num * 4
\name:
	tst.w	CtrlC_enable
	beq.s	o\name
	cmp.b	#3, 0xfffffa2f.w	| CTRL-C from gdb
	bne.s	o\name
	ori.w	#0x700, sr
	move.l	a7, exception_a7
	pea		\num
	jsr		SaveState
	move.w	sr, d0
	andi.w	#0xf8ff, d0
	or.w	srvIrqLevel, d0
	move.w	d0, sr
	/*
		We need to make sure that the system have handled the serial data correctly,
		or we will not be able to use serial com properly in gdbserver.
		So we do a bit of a hack here to fake an irq call to the system handler.
	*/
	move.l	#r\name, -(a7)
	move.w	d0, -(a7)
	jmp		o\name
r\name:
	jsr		Exception
	ori.w	#0x700, sr
	jsr		RestoreState
	move.l	exception_a7, a7
	| We cannot jump to the system handler now as we have stolen the data.
	rte
o\name:
	jmp 	0x12345678
	.endm

/*
	Capture the mfp state, so we can easily detect DCD
*/
	.macro DCD_Hook name, num
	.equ	n\name,	\num * 4
\name:
	move.b	0xfffffa03.w, Mfp_ActiveEdgeRegister
o\name:
	jmp 	0x12345678
	.endm

	.text

	Hook BusError, 2
	Hook AddressError, 3
	Hook IllegalInstruction, 4
	Hook DivisionByZero, 5
	| Hook CHK, 6
	Hook TrapV, 7
	Hook PrivilegeViolation, 8
	Hijack Trace, 9
	Hook NMI, 31
	Hijack BreakPoint, 32
	DCD_Hook MfpDcd, 0x41
	S_Hook SerialInput, 0x4c

	.global ASM_InitExceptions
ASM_InitExceptions:
	.func ASM_InitExceptions
	move.w	sr, -(a7)
	move.w	(a7), d0
	andi.w	#0x700, d0
	move.w	d0, srvIrqLevel
	ori.w	#0x700, sr

	| Get initial value
	move.b	0xfffffa03.w, Mfp_ActiveEdgeRegister

	pea		0.w		| server vectors
	jsr		StoreVectors
	jsr		StoreMemoryRegisters
	lea		4(a7), a7
	
	HookVector	BusError
	HookVector	AddressError
	HookVector	IllegalInstruction
	HookVector	DivisionByZero
	| HookVector	CHK
	HookVector	TrapV
	HookVector	PrivilegeViolation
	HookVector	Trace
	HookVector	NMI
	HookVector	BreakPoint
	HookVector	MfpDcd
	HookVector	SerialInput

	move.w	(a7)+, sr
	moveq	#0, d0
	rts
	.endfunc

	.global ASM_RestoreExceptions
ASM_RestoreExceptions:
	.func ASM_RestoreExceptions
	move.w	sr, -(a7)
	ori.w	#0x700, sr

	UnHookVector	BusError
	UnHookVector	AddressError
	UnHookVector	IllegalInstruction
	UnHookVector	DivisionByZero
	| UnHookVector	CHK
	UnHookVector	TrapV
	UnHookVector	PrivilegeViolation
	UnHookVector	Trace
	UnHookVector	NMI
	
	UnHookVector	BreakPoint
	| UnHookVector	MfpDcd 		| Not needed, it is included in LoadVectors
	| UnHookVector	SerialInput | Not needed, it is included in LoadVectors
	
	pea		0.w		| server vectors
	jsr		LoadVectors
	jsr		LoadMemoryRegisters
	lea		4(a7), a7

	move.w	(a7)+, sr
	moveq	#0, d0
	rts
	.endfunc

SaveState:
	movem.l	d0-d7/a0-a6, registers
	move.l	exception_a7, a0
	move.l	4(a7), d0
	cmp.w	#4, d0
	bge.s	2f
	lea		8(a0), a0		| 7 word exception stack
2:
	move.l	2(a0), registers + (17 * 4)	| pc
	move.w	(a0), d0
	move.l	d0, registers + (16 * 4)	| sr
	lea	6(a0), a1
	btst	#13, d0
	bne.s	1f
	move.l	usp, a1
1:
	move.l	a1, registers + (15 * 4)	| We store the pre-exception stack pointer to show as register
	move.l	a1, pre_exception_a7
	
	pea		1.w		| inferior vectors
	jsr		StoreVectors
	jsr		StoreMemoryRegisters
	lea		4(a7), a7
	pea		0.w		| server vectors
	jsr		LoadVectors
	jsr		LoadMemoryRegisters
	lea		4(a7), a7
	
	rts

RestoreState:
	pea		1.w		| inferior vectors
	jsr		LoadVectors
	jsr		LoadMemoryRegisters
	lea		4(a7), a7

	move.l	registers + (15 * 4), a0
	move.l	registers + (16 * 4), d0	| sr
	btst	#13, d0
	beq.s	2f
	move.l	pre_exception_a7, a1
	cmp.l	a0, a1
	beq.s	3f
	/*
	The person doing the debugging have asked to change the supervisor stack pointer...
	Lets hope he knows what he is doing.
	*/
	move.l	a0, exception_a7
	bra.s	3f
2:
	move.l	a0, usp
3:
	move.l exception_a7, a0
	move.l	4(a7), d1
	cmp.w	#4, d1
	bge.s	1f
	lea		8(a0), a0		| 7 word exception stack
1:
	move.w	d0, (a0)	| sr
	move.l	registers + (17 * 4), 2(a0)	| pc
	movem.l	registers, d0-d7/a0-a6
	rts

CatchException:
	jmp 	0x12345678

	.global ASM_ExceptionSafeMemoryRead
ASM_ExceptionSafeMemoryRead:
	.func ASM_ExceptionSafeMemoryRead
	move.l	a0, -(a7)
	move.w	sr, -(a7)
	move.l	a7, saved_a7
	move.l	#1f, CatchException + 2
	move.l	#CatchException, nBusError
	move.l	#CatchException, nAddressError
	
	move.l	10(a7), a0
	move.b	(a0), d0
	move.l	14(a7), a0
	move.b	d0, (a0)
	moveq	#0, d0
	bra.s	2f
1:
	move.l	saved_a7, a7
	moveq	#1, d0
2:
	move.l	#BusError, nBusError
	move.l	#AddressError, nAddressError
	move.w	(a7)+, sr
	move.l	(a7)+, a0
	rts
	.endfunc

	.global ASM_ExceptionSafeMemoryWrite
ASM_ExceptionSafeMemoryWrite:
	.func ASM_ExceptionSafeMemoryWrite
	move.l	a0, -(a7)
	move.w	sr, -(a7)
	move.l	a7, saved_a7
	move.l	#1f, CatchException + 2
	move.l	#CatchException, nBusError
	move.l	#CatchException, nAddressError
	
	move.l	10(a7), a0
	move.l	14(a7), d0
	move.b	d0, (a0)
	moveq	#0, d0
	bra.s	2f
1:
	move.l	saved_a7, a7
	moveq	#1, d0
2:
	move.l	#BusError, nBusError
	move.l	#AddressError, nAddressError
	move.w	(a7)+, sr
	move.l	(a7)+, a0
	rts
	.endfunc

	.global ASM_CaptureMfpData
ASM_CaptureMfpData:
	.func ASM_CaptureMfpData
	movem.l	d1-d2/a0, -(a7)
	move.l	16(a7), a0

	moveq	#0, d0
	move.w	#10000, d2
1:
	move.b	(a0), d1
	cmp.b	d1, d0
	bhi.s	2f
	move.b	d1, d0
2:
	dbra	d2, 1b

	movem.l	(a7)+, d1-d2/a0
	rts
	.endfunc

	.bss
	.lcomm	exception_a7,		4
	.lcomm	pre_exception_a7,	4
	.lcomm	saved_a7,			4
	.lcomm	srvIrqLevel,		2
	.even
	
