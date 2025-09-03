/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

/*
	This file contains code that is NOT 68000 compatible!
	There are functions that use instructions for 68020+ here.
	Important for anyone using such instructions:
		Always check in code that the computer running the code supports those
		instructions before usage. Like checking the Cookie_CPU or Cookie_FPU. 
*/
	.global registers
	.global Exception
	.global Cookie_CPU
	.global Cookie_VDO
	.global Cookie_FPU
	.global Cookie_MCH
	.global SwitchToServerContext
	.global SwitchToInferorContext

	.equ	o_to_sp, 60
	.equ	o_to_sr, 64
	.equ	o_to_pc, 68
	.equ	o_to_fp0, 72
	.equ	o_to_fp_control, 168
	.equ	o_to_fp_status,	172
	.equ	o_to_fp_iaddr,	176


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
	move.w	#\num, exception_num
	jsr		HandleException
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
	move.w	#\num, exception_num
	jsr		HandleException
	move.l	exception_a7, a7
	rte
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

	Hook FPUnordered, 48
	Hook FPInexact, 49
	Hook FPDivideZero, 50
	Hook FPUnderflow, 51
	Hook FPOperandError, 52
	Hook FPOverflow, 53
	Hook FPSignalingNAN, 54
	Hook FPUnimplemented, 55

	.global InitExceptions
InitExceptions:
	.func InitExceptions
	move.w	sr, -(a7)
	move.w	(a7), d0
	andi.w	#0x700, d0
	move.w	d0, srvIrqLevel
	ori.w	#0x700, sr

	| Get initial value
	move.b	0xfffffa03.w, Mfp_ActiveEdgeRegister

	HookVector BusError
	HookVector AddressError
	HookVector IllegalInstruction
	HookVector DivisionByZero
	| HookVector	CHK
	HookVector TrapV
	HookVector PrivilegeViolation
	HookVector Trace
	HookVector NMI
	HookVector BreakPoint

	move.l	Cookie_FPU, d0
	swap	d0
	and.w	#0x1e, d0
	jeq		1f
	HookVector FPUnordered
	HookVector FPInexact
	HookVector FPDivideZero
	HookVector FPUnderflow
	HookVector FPOperandError
	HookVector FPOverflow
	HookVector FPSignalingNAN
	HookVector FPUnimplemented
	fmove.l	fpcr, d0
	or.w	#0xf0, d0	| fpu exception, enable all exceptions.
	fmove.l	d0, fpcr
1:
	move.w	(a7)+, sr
	moveq	#0, d0
	rts
	.endfunc

	.global RestoreExceptions
RestoreExceptions:
	.func RestoreExceptions
	move.w	sr, -(a7)
	ori.w	#0x700, sr

	UnHookVector BusError
	UnHookVector AddressError
	UnHookVector IllegalInstruction
	UnHookVector DivisionByZero
	| UnHookVector	CHK
	UnHookVector TrapV
	UnHookVector PrivilegeViolation
	UnHookVector Trace
	UnHookVector NMI
	UnHookVector BreakPoint

	move.l	Cookie_FPU, d0
	swap	d0
	and.w	#0x1e, d0
	jeq		1f
	UnHookVector FPUnordered
	UnHookVector FPInexact
	UnHookVector FPDivideZero
	UnHookVector FPUnderflow
	UnHookVector FPOperandError
	UnHookVector FPOverflow
	UnHookVector FPSignalingNAN
	UnHookVector FPUnimplemented
1:
	move.w	(a7)+, sr
	moveq	#0, d0
	rts
	.endfunc

	.global CtrlCException
CtrlCException:
	.func CtrlCException
	move.l	a0, -(a7)
	lea		+8(a7), a0				| Back up to where we were when the exception occured
	move.l	a0, exception_a7
	move.w	#1000, exception_num	| made up ctrl-c exception num
	move.l	(a7)+, a0

	jsr		HandleException
	move.l	exception_a7, a7
	lea		-8(a7), a7				| Forward to where we were when the function was called
	rts
	.endfunc

	.global GetExceptionNum
GetExceptionNum:
	.func GetExceptionNum
	moveq	#0, d0
	move.w	exception_num, d0
	rts
	.endfunc

HandleException:
	jsr		SaveStateAndSwitchContext
	move.w	sr, d0
	andi.w	#0xf8ff, d0
	or.w	srvIrqLevel, d0
	move.w	d0, sr
	jsr		Exception
	ori.w	#0x700, sr
	jmp		RestoreStateAndSwitchContext

/*
	Out:
		a0 = ptr to sr, pc
		a1 = exception ptr before exception call.
*/
Calc68000Stack:
	move.l	exception_a7, a0
    cmp.l   #10, Cookie_CPU
	jpl		Calc68010Stack
	cmp.w	#4, exception_num
	bge.s	2f
	lea		14(a0), a1		| 7 word stack frame
	rts
2:
	lea		6(a0), a1		| 3 word stack frame
	rts
Calc68010Stack:
	cmp.w	#4, exception_num
	bge.s	2f
	lea		58(a0), a1		| 29 word stack frame
	rts
2:
	lea		8(a0), a1		| 4 word stack frame
	rts


SaveStateAndSwitchContext:
	movem.l	d0-d7/a0-a6, registers
	move.l	Cookie_FPU, d0
	swap	d0
	and.w	#0x1f, d0
	jeq		2f
    cmp.l   #20, Cookie_CPU
	jmi		1f
	| Got fpu and a 020+ cpu
	fmovem.x	fp0-fp7, registers + o_to_fp0
	fmove.l		fpcr, registers + o_to_fp_control
	fmove.l		fpsr, registers + o_to_fp_status
	fmove.l		fpiar, registers + o_to_fp_iaddr
	jra		2f
1:
	btst	#0, d0
	jeq		2f
	| ToDo: SFP004

2:
	jsr		Calc68000Stack
	move.l	a0, exception_sr_pc
	move.l	2(a0), registers + o_to_pc
	move.w	(a0), d0
	move.l	d0, registers + o_to_sr
	btst	#13, d0
	bne.s	1f
	move.l	usp, a1
1:
	move.l	a1, registers + o_to_sp
	move.l	a1, pre_exception_a7

	jsr		SwitchToServerContext
	
	rts

RestoreStateAndSwitchContext:
	jsr		SwitchToInferiorContext

	move.l	registers + o_to_sp, a0
	move.l	registers + o_to_sr, d0
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
	jsr		Calc68000Stack
	move.w	d0, (a0)	| sr
	move.l	registers + o_to_pc, 2(a0)
	move.l	Cookie_FPU, d0
	swap	d0
	and.w	#0x1f, d0
	jeq		2f
    cmp.l   #20, Cookie_CPU
	jmi		1f
	| Got fpu and a 020+ cpu
	fmovem.x	registers + o_to_fp0, fp0-fp7
	fmove.l		registers + o_to_fp_control, fpcr
	fmove.l		registers + o_to_fp_status, fpsr
	fmove.l		registers + o_to_fp_iaddr, fpiar
	jra		2f
1:
	btst	#0, d0
	jeq		2f
	| ToDo: SFP004
	
2:
	movem.l	registers, d0-d7/a0-a6
	rts

	.global ExceptionSafeMemoryRead
ExceptionSafeMemoryRead:
	.func ExceptionSafeMemoryRead
	move.l	a0, -(a7)
	move.w	sr, -(a7)
	move.l	a7, saved_a7
	move.l	#1f, nBusError
	move.l	#1f, nAddressError
	
	move.l	10(a7), a0
	move.b	(a0), d0
	move.l	14(a7), a0
	move.b	d0, (a0)
	moveq	#0, d0
	bra.s	2f
1:
	/*
		If we have an exception, we simply restore the stack pointer and don't care about
		checking what's in the stack frame.
		We know that something went wrong and that is all we need to know.
	*/
	move.l	saved_a7, a7
	moveq	#1, d0
2:
	move.l	#BusError, nBusError
	move.l	#AddressError, nAddressError
	move.w	(a7)+, sr
	move.l	(a7)+, a0
	rts
	.endfunc

	.global ExceptionSafeMemoryWrite
ExceptionSafeMemoryWrite:
	.func ExceptionSafeMemoryWrite
	move.l	a0, -(a7)
	move.w	sr, -(a7)
	move.l	a7, saved_a7
	move.l	#1f, nBusError
	move.l	#1f, nAddressError
	
	move.l	10(a7), a0
	move.l	14(a7), d0
	move.b	d0, (a0)
	moveq	#0, d0
	bra.s	2f
1:
	/*
		If we have an exception, we simply restore the stack pointer and don't care about
		checking what's in the stack frame.
		We know that something went wrong and that is all we need to know.
	*/
	move.l	saved_a7, a7
	moveq	#1, d0
2:
	move.l	#BusError, nBusError
	move.l	#AddressError, nAddressError
	move.w	(a7)+, sr
	move.l	(a7)+, a0
	rts
	.endfunc

	.global CaptureMfpData
CaptureMfpData:
	.func CaptureMfpData
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

	.global	ClearInternalCaches
ClearInternalCaches:
	.func ClearInternalCaches
    cmp.l   #20, Cookie_CPU
    jmi     1f
    move.l  d1, -(a7)
    movec	cacr, d1
    bset	#3, d1
    bset	#11, d1
	movec	d1, cacr
    move.l  (a7)+, d1
1:
    rts
	.endfunc

	.bss
	.lcomm	exception_a7,		4
	.lcomm	exception_num,		2
	.lcomm	pre_exception_a7,	4
	.lcomm	exception_sr_pc,	4
	.lcomm	saved_a7,			4
	.lcomm	srvIrqLevel,		2
	.even
	
