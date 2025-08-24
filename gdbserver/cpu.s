
	.global Cookie_CPU

	.text
	.global	ClearInternalCaches
ClearInternalCaches:
	.func ClearInternalCaches
    cmp.l   #20, Cookie_CPU
    jmi     1f
    move.l  d1, -(a7)
	.dc.l	0x4e7a1002      | movec	cacr, d1
    bset	#3, d1
    bset	#11, d1
	.dc.l	0x4e7b1002      | movec	d1, cacr
    move.l  (a7)+, d1
1:
    rts
