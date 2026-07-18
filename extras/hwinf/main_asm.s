
	.text

	.global hw_detect
	.global draw_wakestate
	.global cookie_cpu
	.global cookie_fpu
	.global cookie_vdo
	.global cookie_mch

hw_detect:
	/*
		Get cookies.
		There's a lot of useful information we can get for free.
	*/
	bsr		get_cookies

	/*
		Save some data so we can restore it at exit.
	*/
	move.w	sr, -(a7)
	move.w	#0x2700, sr
	move.b	0xfffffa07.w, -(a7)
	move.b	0xfffffa09.w, -(a7)
	move.b	0xfffffa13.w, -(a7)
	move.b	0xfffffa15.w, -(a7)
	move.w	0xffff8240.w, -(a7)
	move.b	0xffff8260.w, -(a7)
	move.b	0xffff820a.w, -(a7)
	lea		0x64.w, a0
	moveq	#6, d0
1:
	move.l	(a0),	-(a7)
	move.l	#just_rte,	(a0)+	| Default irq is just a rte.
	dbra	d0, 1b

/*
	Set the vbl irq to the routine that will detect the
	hardware state of the machine.
	This routine will only run once, and after that	"vbl_irq" will
	be the vbl irq rutine that runs.
*/
	move.l	#analyze_hardware_init, 0x70.w
	clr.b	0xfffffa07.w
	clr.b	0xfffffa09.w
	clr.b	0xfffffa13.w
	clr.b	0xfffffa15.w
	bsr		fill_test_pattern
main_loop:
/*
	By making a stop, we make sure that the E cycle is
	the only thing affecting the irq.
*/
	stop	#0x2300
/*
	We get here after vbl irq is finished.
*/
	move.w	failed_e, d0	| set if timing value wasn't in the list.
	bne.s	2f
/*
	Do a (bad) check for released space key.
*/
	cmp.b	#0xb9, 0xfffffc02.w
	bne.s	main_loop
2:
/*
	Restore state and exit.
*/
	move.w	#0x2700, sr
	lea		0x80.w, a0
	moveq	#6, d0
1:
	move.l	(a7)+, -(a0)
	dbra	d0, 1b
	move.b	(a7)+, 0xffff820a.w
/*
	Wait for vbl to reset the screen resolution correct
*/
	lea		0xffff8201.w, a0
	movep.w	0(a0), d0
	lea		0xffff8205.w, a0
1:
	movep.w	0(a0), d1
	cmp.w	d0, d1
	bne.s	1b

	move.b	(a7)+, 0xffff8260.w
	move.w	(a7)+, 0xffff8240.w
	move.b	(a7)+, 0xfffffa15.w
	move.b	(a7)+, 0xfffffa13.w
	move.b	(a7)+, 0xfffffa09.w
	move.b	(a7)+, 0xfffffa07.w
	move.w	(a7)+, sr
	moveq	#0, d0
	move.w	wakestate, d0
	rts

just_rte:
	rte

/*
	Draw a test pattern that can be used for checking the
	position of the red test bar.
*/
fill_test_pattern:
	moveq	#0, d0
	lea		0xffff8201.w, a0
	movep.w	0(a0), d0
	lsl.l	#8, d0
	move.l	d0, a0
	move.l	d0, vidadd
	move.w	#200 - 1, d0
1:
	move.w	#10 - 1, d1
2:
	move.l	#-1, (a0)+
	clr.l	(a0)+
	clr.l	(a0)+
	clr.l	(a0)+
	dbra	d1, 2b
	dbra	d0, 1b
	rts

	.lcomm	vidadd, 4

/*
	Draw a number of columns that can be used to visually check
	the wakestate this routine have detected.
	1 column = STE
	2 columns = wakestate 1
	3 columns = wakestate 2
	4 columns = wakestate 3
	5 columns = wakestate 4
	6 columns = failed detecting wakestate
*/
draw_wakestate:
	move.l	vidadd, a0
	move.w	#10 - 1, d0
1:
	move.w	wakestate, d1
	move.l	a0, a1
2:
	move.l	#0xffff0000, (a1)+
	move.l	#0xffff0000, (a1)+
	dbra	d1, 2b
	lea		160(a0), a0
	dbra	d0, 1b
	rts

get_cookies:
	move.l	0x5a0.w, d0
	beq.s	2f
	move.l	d0, a0
1:
	movem.l	(a0)+, d0-d1
	tst.l	d0
	beq.s	2f
	cmp.l	#0x5f4d4348, d0	| '_MCH'
	bne.s	3f
	move.l	d1, cookie_mch
	bra.s	1b
3:
	cmp.l	#0x5f435055, d0	| '_CPU'
	bne.s	3f
	move.l	d1, cookie_cpu
	bra.s	1b
3:
	cmp.l	#0x5f56444f, d0	| '_VDO'
	bne.s	3f
	move.l	d1, cookie_vdo
	bra.s	1b
3:
	cmp.l	#0x5f465055, d0	| '_FPU'
	bne.s	3f
	move.l	d1, cookie_fpu
3:
	bra.s	1b
2:
	rts

	.bss
	.lcomm	cookie_cpu, 4
	.lcomm	cookie_fpu, 4
	.lcomm	cookie_vdo, 4
	.lcomm	cookie_mch, 4
