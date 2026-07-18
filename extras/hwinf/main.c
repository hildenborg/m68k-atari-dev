/*
	Example code for detecting hardware information and state.
	It will detect ST wakestates and 68000 E clock to mem cycle alignment.
	This will make it possible to predict the irq jitter for the VBL irq
	and sync the cpu to the raster beam at line 0.

Running:
	int wakestate = Supexec(hw_detect);
	Will return wakestate and set:
	extern unsigned short wakestate;
	extern unsigned int	cookie_cpu;
	extern unsigned int	cookie_fpu;
	extern unsigned int	cookie_vdo;
	extern unsigned int	cookie_mch;

	If E cycle align fails, then:
	extern unsigned short failed_e;
	is not zero and set to the measured timing value.
	This should be reported!

	The intresting code is in "hw_detect.s".
	And "vbl_irq" is the function that becomes the jitter free vbl interrupt.

Acknowledgements:
	This code would not have been possible without the huge amount of research done by:
	Troed
	Ijor
	Dio
	Paulo Simoes
	Alien of ST Connexion
	And a lot more, specifically people in this forum thread: https://www.atari-forum.com/viewtopic.php?f=16&t=24855&start=147

	The wakestate detection code in "hw_detect.s" is based on Troeds WSDETECT: https://codeberg.org/troed/WSDETECT

	The E cycle alignment timing using ACIA reads was discovered by TFE/Omega.

*/
#include <stdio.h>
#include <tos.h>

extern int hw_detect(void);
/*
	wakestate:
	0 = STE
	1 = ST wakestate 1
	2 = ST wakestate 2
	3 = ST wakestate 3
	4 = ST wakestate 4
	5 = failed detecting wakestate
*/
extern unsigned short wakestate;
/*
	failed_e is set to the measured E cycle timing if it wasn't found in list of known timings. 
*/
extern unsigned short failed_e;
/* 
	cookie_cpu (low word) = cpu: 
	0 = 68000
	10 = 68010
	20 = 68020
	30 = 68030
	40 = 68040
	60 = 68060
*/
extern unsigned int	cookie_cpu;
/*
	cookie_fpu (high word) = fpu:
	bit 0 = SFP004
	bits 1 - 2 =
		01	= 68881 or 68882
		10	= 68881
		11	= 68882
	bit 3 = 68040 Internal
	bit 4 = 68060 Internal
*/
extern unsigned int	cookie_fpu;
/*
	cookie_vdo (high word) = shifter:
	0 = st
	1 = ste
	2 = tt
	3 = falcon
*/
extern unsigned int	cookie_vdo;
/*
	cookie_mch (long word) = machine:
	0x00000 = st
	0x10000 = ste
	0x10008 = st book
	0x10010 = mega ste
	0x20000 = tt
	0x30000 = falcon
*/
extern unsigned int	cookie_mch;

int main (int argc, char **argv)
{
	/*
		This will detect the wakestate and E cycle align.
		A test screen will be shown until the space key is pressed.
	*/
	int wakestate = Supexec(hw_detect);
	printf("\33E");	// GEMDOS VT52 clear screen and put cursor at top left.
	if (wakestate == 0)
	{
		printf("STE have no wakestates.\n");
	}
	else if (wakestate >= 1 && wakestate < 5)
	{
		printf("ST with wakestate: ws%d\n", wakestate);
	}
	else
	{
		printf("Unknown wakestate!\n");
	}
	if (failed_e != 0)
	{
		printf("E cycle timing unknown!\nPlease report: 0x%04x timing.\n", failed_e);
	}
	printf("Press any key...\n");
	// Wait for keypress
	while (Bconstat(DEV_CON) == 0)
	{
	}
	return 0;	
}