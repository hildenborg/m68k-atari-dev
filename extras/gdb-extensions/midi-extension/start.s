/*
	Copyright (C) 2026 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

	/*
		__BSS_SEGMENT_END is defined in "link.ld".
		It is the address for the first free byte after our program.
	*/
	.global __BSS_SEGMENT_END
	.global GdbSrvComStruct

	.section	".init"
	.global	_start
	.type	_start,#function
_start:
	/*
		4(a7) contains a pointer to the basepage structure.
		That structure is immediately followed by this program code.
	*/
	move.l	4(a7), _BasePage
	/*
		We save the current end of TPA.
		We might need to change this depending on cookie space.
	*/
	move.l	#__BSS_SEGMENT_END, _TPA_end
	/*
		Set gdb plugin cookie
	*/
	pea		SetCookie
	move.w	#0x26, -(a7)
	trap	#14
	addq.l	#6, a7
	tst.w	d0
	beq.s	CookieSuccess
	/*
		Failure, exit as such.
	*/
	move.w	d0, -(a7)
	move.w	#0x4c, -(a7)
	trap	#1

CookieSuccess:
	/*
		Exit using the Ptermres call.
		This exits this program but lets it stay in memory.
	*/
	move.l	_TPA_end, d0
	sub.l	_BasePage, d0
	/*
		d0 now contains the number of bytes this program occupies in memory.
	*/
	move.w	#0,-(sp)
	move.l	d0,-(sp)
	move.w	#0x31,-(sp)
	trap	#1
	/*
		Never gets here.
	*/

SetCookie:
	/*
		Get address to cookie array.
	*/
	move.l	0x5a0.w, a0	| Cookie array
	moveq	#0, d0		| Cookie count
GetNextCookie:
	addq.w	#1, d0
	move.l	(a0), d1
	beq.s	FoundLastCookie
	cmp.l	Cookie_SDBG, d1
	beq.s	FoundSameCookie
	addq.l	#8, a0
	bra.s	GetNextCookie
FoundSameCookie:
	/*
		Problem: there is already someone using the cookie.
		We have to exit with a failure.
	*/
	moveq	#-1, d0
	rts

FoundLastCookie:
	/*
		Is there any more space in the array?
		If not, get more cookie space.
	*/
	cmp.l	4(a0), d0
	beq.s	NeedMoreCookies
PutCookie:
	/*
		We can put our cookie in this position.
	*/
	move.l	Cookie_SDBG, (a0)
	move.l	#GdbSrvComStruct, 4(a0)
	moveq	#0, d0
	rts

NeedMoreCookies:
	/*
		Copy existing cookies.
	*/
	move.l	d0, d1
	move.l	0x5a0.w, a0
	move.l	_TPA_end, a1
	bra.s	2f
1:
	move.l	(a0)+, (a1)+
	move.l	(a0)+, (a1)+
2:
	dbra	d0, 1b
	lea		-8(a1), a0	| Save slot position for our cookie.

	/*
		Add four slots.
		Mark last slot as last slot.
	*/
	clr.l	(a1)+
	clr.l	(a1)+
	clr.l	(a1)+
	clr.l	(a1)+
	clr.l	(a1)+
	clr.l	(a1)+
	clr.l	(a1)+
	addq.l	#4, d1
	move.l	d1, (a1)+	| Last slot value is equal to number of slots.
	/*
		Set new cookie array address and save our new TPA end.
	*/
	move.l	_TPA_end, 0x5a0.w
	move.l	a1, _TPA_end
	bra		PutCookie

	.data
	.even
Cookie_SDBG:
	.asciz	"SDBG"
	.even

	.bss
	.lcomm 	_BasePage, 4
	.lcomm	_TPA_end, 4
