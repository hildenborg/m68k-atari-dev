	.text

	.global	wakestate
	.global	failed_e
	.global analyze_hardware_init

/*
	Macro for waiting a specific number of nops.
	Be careful to not use local label "9" in connection to this macro.
*/
	.macro WaitNops num, reg
	.if	\num < 5
		.rept	(\num & 1)
		nop
		.endr
		.rept	(\num >> 1)
		or.l	d0, d0
		.endr
	.else
		.set	mNum, (\num - 2)	| -1 for moveq, -1 for dbra end
		.set	lNum, (mNum / 3)
		.set	nNum, (mNum % 3)
		.rept	(nNum & 1)
		nop
		.endr
		.rept	(nNum >> 1)
		or.l	d0, d0
		.endr
		moveq	#lNum - 1, \reg		| 1 nop
	9:
		dbra	\reg, 9b			| 3 nops when loop, 4 nops when end
	.endif
	.endm

analyze_hardware_init:
/*
	This makes sure that we doesn't have a late vbl irq.
*/
	move.l	#analyze_hardware_state, 0x70.w
	rte

analyze_hardware_state:
	move.w	#0x2700, sr
	movem.l	d0-d5/a0-a3, -(a7)

	lea		0xffff8209.w, a0
	lea		0xffff820a.w, a1
	lea		0xffff8260.w, a2
	lea		0xfffffc00.w, a3
	moveq	#0, d0
	moveq	#2, d2

/*
	Check if shifter is STE.
*/
	move.w	cookie_vdo, d4
	cmp.w	#1, d4
	seq		d4		| d4.b clear if not STE
/*
	Read video address and save it for later.
*/
	moveq	#0, d1
	movep.l	-6(a0), d1
	and.l	#0xffffff, d1

/*
	Standard raster beam sync.
*/
1:
	move.b	(a0), d3
	beq.s	1b
	neg.b	d3			| neg to keep it at even number of shifts
	lsl.l	d3, d3		| 8 + shift*2 cycles

/*
	Repeat two times:
		Read from ACIA, which will take 16 to 24 clock cycles depending on E clock alignment.
		Read from low video pointer to get timing.
*/
	move.b	(a3), d3
	move.b	(a0), d5
	move.b	(a3), d3
	move.b	(a0), d3
	lsl.w	#8, d5
	move.b	d3, d5
/*
	Timing value needed to detect E cycle is now in d5.
	Continue with detecting ST wakestates.
*/
	tst.b	d4
	bne		wakestate_found		| no wakestate for STE (assume that d0 is 0)
/*
	Sync again and do ST wakestate detect.
	We don't have to wait for the video pointer to change as the raster beam
	is drawing graphics at this time.
	But as the shift instruction shifts with modulo 64 steps, we need to
	use an offset to not accidently flip between a large and small shift.
*/
	move.w	#0xa0, d3
	sub.b	(a0), d3
	lsl.l	d3, d3
| cycle 396

	WaitNops	38, d4

| cycle 36
	move.b d0,(a1)
	exg d0,d0
	move.b	d2,(a1)
| cycle 60

	WaitNops	26, d4

| cycle 164
	move.b	d2, (a2)
	move.b	d0, (a2)

	WaitNops	47, d4
	exg d0,d0

| cycle 374
	move.b	d2, (a2)
	move.b	d0, (a2)
| cycle 392

	WaitNops	23, d4

/*
	Read video address again.
*/
| cycle 484
	movep.l	-6(a0), d3	| get video address
| cycle 0

/*
	End line manipulation with stabilizer code to get
	glue and shifter back into a stable state.
	We do this by opening the left and right border on one line
	using the timing and stabilizer code found at: https://ae.dhs.nu/hatari_overscan/
*/
	nop
	move.b	d2, (a2)
	move.b	d0, (a2)

	WaitNops	88, d4

| cycle 372
	move.b	d0, (a1)
	move.b	d2, (a1)

	WaitNops	11, d4

| cycle 432
	move.b	d2, (a2)
	move.b	d0, (a2)

/*
	Subtract the first from second address read.
	The result should be one 160 byte line + the length of the modified line.
*/
	and.l	#0xffffff, d3
	sub.l	d1, d3
	sub.w	#160, d3
/*
	d3 is the line length of the modified line in bytes.
	Compare it against known lengths for different wakestates.
*/
	lea		wakestate_line_lengths, a0
	moveq	#1, d0
1:
	cmp.w	(a0)+, d3
	beq.s	wakestate_found
	addq.w	#1, d0
	cmp.w	#5, d0
	bne.s	1b
	/*
		Unknown wakestate.
		Save it as wake state 5 so we can test for it.
	*/
wakestate_found:
	move.w	d0, wakestate
	lea		e_delay, a0
	lsl.w	#3, d0
	add.w	d0, a0
	move.l	a0, ws_ptr

/*
	Find the timed E clock in a list of five known timings.
*/
	lea		e_times, a0
1:
	move.w	(a0)+, d1
	move.w	(a0)+, d0
	bmi.s	3f
	cmp.w	d1, d5
	bne.s	1b
	bra.s	2f
3:
	/*
		This timing is unknown.
		Just make it up instead of crashing.
	*/
	move.w	d5, failed_e
	moveq	#0, d0
	lea		e_delay + (5 * 8), a0
	move.l	a0, ws_ptr
2:
/*
	Found the timing, set the E clock frame counter.
*/
	move.w	d0, e_frame
	bsr		draw_wakestate
	movem.l	(a7)+, d0-d5/a0-a3
	move.l	#vbl_irq, 0x70.w
	rte

vbl_irq:
	move.w	#0x2700, sr
	movem.l	d0-d4/a0, -(a7)
	move.l	ws_ptr, a0		| The row in e_delay for the detected wakestate.
	add.w	e_frame, a0	| E cycle frame.
	move.b	(a0), d0		| Fetch the delay needed for this E cycle frame.
	lsl.l	d0, d0			| Delay.

/*
	At this point, we are synced to the raster beam.
	Draw a vertical color bar.
*/
	moveq	#0, d0
	move.w	#0xf00, d1
	lea		0xffff8240.w, a0

	move.w	#250, d2
1:
	WaitNops	121, d4
	move.w	d1, (a0)
	move.w	d0, (a0)
	dbra	d2, 1b

/*
	Update E clock frame counter.
	This is done at the end as its use of clock cycles varies.
	Missing updating this counter will break raster beam sync to vbl irg.
*/
	move.w	e_frame, d0
	addq.w	#1, d0
	cmp.w	#5, d0
	bne.s	1f
	moveq	#0, d0
1:
	move.w	d0, e_frame
	movem.l	(a7)+, d0-d4/a0
	rte

	.data
/*
	Known timings for E clock alignment.
	Timing + index (0-4).
	If new timings are found, then you need to try by hand to test which index it should have.
*/
e_times:
	.dc.w	0x5666, 0
	.dc.w	0x5868, 1
	.dc.w	0x5664, 2
	.dc.w	0x5866, 3
	.dc.w	0x5464, 4
	.dc.w	0x5a68, 4
	.dc.w	-1,-1


/*
	Known line length in bytes depending on wakestate.
*/
wakestate_line_lengths:
	.dc.w	162		| wakestate 1
	.dc.w	54		| wakestate 2
	.dc.w	206		| wakestate 3
	.dc.w	204		| wakestate 4

/*
	Delay time for a specific E clock frame and wakestate.
	This list is made to sync the raster beam to the same place on
	all wakestates + ste.
*/
e_delay:
	.dc.b	0,3,1,3,1,0,0,0	| ste
	.dc.b	5,7,5,4,7,0,0,0	| ws1
	.dc.b	5,3,5,3,2,0,0,0	| ws2
	.dc.b	5,3,5,3,2,0,0,0	| ws3
	.dc.b	5,3,5,3,2,0,0,0	| ws4
	.dc.b	0,0,0,0,0,0,0,0	| failed ws, delay is useless.
	.even

	.bss
	.lcomm	ws_ptr, 4
	.lcomm	e_frame, 2		| 0 to 4. Used to determine the alignment of E cycles to m68k mem cycles.
	.lcomm	wakestate, 2	| 0 to 5. 0 = ste, 5 = unknown. 1-4 = st wakestates
	.lcomm	failed_e, 2
