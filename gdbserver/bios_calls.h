#ifndef BIOS_CALLS_DEFINED
#define BIOS_CALLS_DEFINED

#include "gem_basepage.h"

#ifdef __cplusplus
extern "C" {
#endif

unsigned int Dsetdrv(unsigned short bios_drive);

int Dsetpath(const char* bios_path);

int Mfree(void* start_addr);

#define PE_LOADGO		0
#define PE_LOAD			3
#define PE_GO			4
#define PE_BASEPAGE		5
#define PE_GOTHENFREE	6
int Pexec(unsigned short mode, const char* file_name, const char* cmdline, const char* envstring);

void __attribute__ ((noreturn)) Pterm(unsigned short retcode);

#define DEV_PRINTER		0
#define DEV_AUX			1
#define DEV_CONSOLE		2
#define DEV_CON			2
#define DEV_MIDI		3
#define DEV_IKBD		4
#define DEV_RAW			5
int Bconstat(unsigned short dev);

unsigned int Bconin(unsigned short dev);

unsigned int Bconout(unsigned short dev, unsigned short ch);

int Bcostat(unsigned short dev);

// speed
#define BAUD_19200		0
#define BAUD_9600		1
#define BAUD_4800		2
#define BAUD_3600		3
#define BAUD_2400		4
#define BAUD_2000		5
#define BAUD_1800		6
#define BAUD_1200		7
#define BAUD_600		8
#define BAUD_300		9
#define BAUD_200		10
#define BAUD_150		11
#define BAUD_134		12
#define BAUD_110		13
#define BAUD_75			14
#define BAUD_50			15
#define BAUD_INQUIRE	0xfffe
#define RS_LASTBAUD		0xfffe
// flow
#define FLOW_NONE		0
#define FLOW_SOFT		1
#define FLOW_HARD		2
#define FLOW_BOTH		3
// ucr
#define RS_ODDPARITY	0x0002
#define RS_EVENPARITY	0x0000
#define RS_PARITYENABLE	0x0004
#define RS_NOSTOP		0x0000
#define RS_1STOP		0x0008
#define RS_15STOP		0x0010
#define RS_2STOP		0x0018
#define RS_8BITS		0x0000
#define RS_7BITS		0x0020
#define RS_6BITS		0x0040
#define RS_5BITS		0x0060
#define RS_CLK16		0x0080
// rsr, tsr
#define RS_RECVENABLE	0x0001
#define RS_SYNCSTRIP 	0x0002
#define RS_MATCHBUSY 	0x0004
#define RS_BRKDETECT 	0x0008
#define RS_FRAMEERR  	0x0010
#define RS_PARITYERR 	0x0020
#define RS_OVERRUNERR	0x0040
#define RS_BUFFULL   	0x0080
// ucr, rsr, tsr, scr
#define RS_INQUIRE   	0xffff
int Rsconf(unsigned short speed, unsigned short flow, unsigned short ucr, unsigned short rsr, unsigned short tsr, unsigned short scr);


#define GI_FLOPPYSIDE	0x01
#define GI_FLOPPYA		0x02
#define GI_FLOPPYB		0x04
#define GI_RTS			0x08
#define GI_DTR			0x10
#define GI_STROBE		0x20
#define GI_GPO			0x40
#define GI_SCCPORT		0x80
void Offgibit(unsigned short mask);

void Ongibit(unsigned short mask);

int Supexec(int (*callback)(void));

#ifdef __cplusplus
}
#endif

#endif // BIOS_CALLS_DEFINED
