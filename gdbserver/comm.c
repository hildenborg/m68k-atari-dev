/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#include "comm.h"
#include "bios_calls.h"
#include "server.h"
#include "clib.h"
#include "cookies.h"

volatile short CtrlC_enable;
volatile unsigned char Mfp_ActiveEdgeRegister;

void InitMfpAux(_CommException CommException);
void ExitMfpAux(void);

void InitSccAux(_CommException CommException);
void ExitSccAux(void);

comm*	comDev = 0;

int GetByte(void)
{
	int byte;

	while ((byte = comDev->ReceiveByte()) == COMM_ERR_NOT_READY)
	{
		if (CheckServerQuitKey() < 0)
		{
			byte = COMM_ERR_KILLED;
			break;
		}
	}
	return byte;
}

void PutByte(char ch)
{
	while (comDev->TransmitByte(ch) == COMM_ERR_NOT_READY)
	{
	}
}


bool Mfp_IsMyDevice(const char *comString)
{
	if ((Cookie_MCH >> 16) < 3 && StringCompare("AUX", comString) >= 0)
	{
		return true;
	}
	return false;
}


/*
	The default TOS serial input buffer is only 256 bytes. A gdb packet is up to 1023 bytes
	(more on the wire when escaped), and arrives as one burst. Without working flow control
	(e.g. emulators, or cables without RTS/CTS), anything above 256 bytes that arrives faster
	than we read it is lost. So the TOS buffer is replaced with a larger one while the server runs.
*/
#define SERIAL_INPUT_BUFFER_SIZE 2048
unsigned char	serialInputBuffer[SERIAL_INPUT_BUFFER_SIZE];
struct IOREC	originalSerialInput;	// The TOS buffer record, restored on exit.
struct IOREC*	serialInput = 0;

// Must be called in supervisor mode.
static unsigned short DisableIrq(void)
{
	unsigned short sr;
	__asm__ volatile ("move.w %%sr, %0\n\tori.w #0x700, %%sr" : "=d" (sr) : : "memory", "cc");
	return sr;
}

// Must be called in supervisor mode.
static void RestoreIrq(unsigned short sr)
{
	__asm__ volatile ("move.w %0, %%sr" : : "d" (sr) : "memory", "cc");
}

// Field by field copy, as a struct assignment would need memcpy, which we don't have.
static void CopyIorec(struct IOREC* dst, const struct IOREC* src)
{
	dst->ibuf = src->ibuf;
	dst->ibufsiz = src->ibufsiz;
	dst->ibufhd = src->ibufhd;
	dst->ibuftl = src->ibuftl;
	dst->ibuflow = src->ibuflow;
	dst->ibufhi = src->ibufhi;
}

// Replaces the TOS serial input buffer with ours. Must be called in supervisor mode.
static void SetSerialInputBuffer(void)
{
	serialInput = Iorec(IO_SERIAL);
	unsigned short sr = DisableIrq();
	CopyIorec(&originalSerialInput, serialInput);
	serialInput->ibuf = serialInputBuffer;
	serialInput->ibufsiz = SERIAL_INPUT_BUFFER_SIZE;
	serialInput->ibufhd = 0;
	serialInput->ibuftl = 0;
	serialInput->ibuflow = SERIAL_INPUT_BUFFER_SIZE / 4;		// Flow control: resume sending below this
	serialInput->ibufhi = (SERIAL_INPUT_BUFFER_SIZE * 3) / 4;	// Flow control: stop sending above this
	RestoreIrq(sr);
}

// Puts back the TOS serial input buffer. Must be called in supervisor mode.
static void RestoreSerialInputBuffer(void)
{
	if (serialInput != 0)
	{
		unsigned short sr = DisableIrq();
		CopyIorec(serialInput, &originalSerialInput);
		// Any data left is in our buffer, so start with an empty TOS buffer.
		serialInput->ibufhd = 0;
		serialInput->ibuftl = 0;
		RestoreIrq(sr);
		serialInput = 0;
	}
}

/*
	Rsconf speed codes are the index in this table, for both the MFP and the SCC (BAUD_19200 is 0).
	19200 is the fastest rate Rsconf has.
*/
static const unsigned short rsconfRates[] = {19200, 9600, 4800, 3600, 2400, 2000, 1800, 1200, 600, 300, 200, 150, 134, 110, 75, 50};
static unsigned short serialBaud = BAUD_19200;

bool SetSerialBaud(unsigned int baud)
{
	for (unsigned short i = 0; i < sizeof(rsconfRates) / sizeof(rsconfRates[0]); ++i)
	{
		if (rsconfRates[i] == baud)
		{
			serialBaud = i;
			return true;
		}
	}
	return false;
}

// Sets the speed (default 19200) and 8N1 with RTS/CTS, gives the AUX device a larger input buffer and empties it.
// Must be called in supervisor mode.
static void AuxInit(void)
{
	Rsconf(serialBaud, FLOW_HARD, RS_CLK16 | RS_1STOP | RS_8BITS, RS_INQUIRE, RS_INQUIRE, RS_INQUIRE);
	SetSerialInputBuffer();
	while (Bconstat(DEV_AUX) != 0)
	{
		Bconin(DEV_AUX);
	}
}

int Mfp_Init(const char *comString, _CommException CommException)
{
	InitMfpAux(CommException);
	AuxInit();
	
	// Set DTR to ON
	Ongibit(GI_DTR);

	return 0;
}


void Mfp_Exit(void)
{
	// Set DTR to OFF
	Offgibit(GI_DTR);
	ExitMfpAux();
	RestoreSerialInputBuffer();
}


bool Mfp_IsConnected(void)
{
	return (Mfp_ActiveEdgeRegister & 0x02) == 0;
}


// Number of bytes in a TOS ring buffer.
static short RingCount(const struct IOREC* ring)
{
	short count = ring->ibuftl - ring->ibufhd;
	return count < 0 ? count + ring->ibufsiz : count;
}

/*
	Sends a byte through the BIOS AUX device, which TOS maps to the MFP or SCC port.
	Each BIOS call is a trap, which costs a lot per byte. So when there is room in the
	TOS output buffer (which follows the input buffer record), Bconout is called directly,
	as it will not block. Only a full buffer needs Bcostat and the connection check.
*/
static int AuxTransmitByte(unsigned char byte, _IsConnected isConnected)
{
	if (serialInput != 0)
	{
		const struct IOREC* output = serialInput + 1;
		if (RingCount(output) < output->ibufsiz - 2)
		{
			Bconout(DEV_AUX, (unsigned short)byte);
			return 0;
		}
	}
	if (Bcostat(DEV_AUX) == 0)
	{
		if (!isConnected())
		{
			return COMM_ERR_DISCONNECTED;
		}
		return COMM_ERR_NOT_READY;
	}
	Bconout(DEV_AUX, (unsigned short)byte);
	return 0;
}

/*
	Receives a byte through the BIOS AUX device, which TOS maps to the MFP or SCC port.
	To avoid two BIOS traps per byte, the byte is read directly from our input buffer
	while it holds less than the low water mark. TOS only does flow control (RTS) near
	the high water mark, and above the low water mark Bconin is used, so TOS can turn
	RTS back on. Like TOS, the head index is advanced first and then the byte is read.
*/
static int AuxReceiveByte(_IsConnected isConnected)
{
	struct IOREC* input = serialInput;
	if (input != 0 && RingCount(input) < input->ibuflow)
	{
		if (input->ibufhd == input->ibuftl)
		{
			return isConnected() ? COMM_ERR_NOT_READY : COMM_ERR_DISCONNECTED;
		}
		short head = input->ibufhd + 1;
		if (head >= input->ibufsiz)
		{
			head = 0;
		}
		unsigned char byte = input->ibuf[head];
		input->ibufhd = head;	// Written after the read, so the interrupt can't overwrite the byte.
		return byte;
	}
	if (Bconstat(DEV_AUX) == 0)
	{
		if (!isConnected())
		{
			return COMM_ERR_DISCONNECTED;
		}
		return COMM_ERR_NOT_READY;
	}
	return Bconin(DEV_AUX) & 0xff;
}

int Mfp_TransmitByte(unsigned char byte)
{
	return AuxTransmitByte(byte, Mfp_IsConnected);
}

int Mfp_ReceiveByte(void)
{
	return AuxReceiveByte(Mfp_IsConnected);
}


void SetCtrlCFlag(bool enable)
{
	CtrlC_enable = enable ? 1 : 0;
}

const char*	Mfp_DeviceName(void)
{
	return "MFP serial device.\r\n";
}

comm MfpCom =
{
	1,
	Mfp_IsMyDevice,
	Mfp_DeviceName,
	Mfp_Init,
	Mfp_Exit,
	Mfp_TransmitByte,
	Mfp_ReceiveByte,
	Mfp_IsConnected,
	SetCtrlCFlag
};

comm* GetMfpSerial(void)
{
	return &MfpCom;
}

bool Scc_IsMyDevice(const char *comString)
{
	if ((Cookie_MCH >> 16) == 3 && StringCompare("AUX", comString) >= 0)
	{
		return true;
	}
	return false;
}

int		sccOldBconmap = -1;	// The AUX mapping to restore on exit.

int Scc_Init(const char *comString, _CommException CommException)
{
	// The Falcon's TOS maps AUX to the SCC channel B port by default, but make sure.
	sccOldBconmap = Bconmap(BCONMAP_SCC_B);
	InitSccAux(CommException);
	AuxInit();
	return 0;
}

void Scc_Exit(void)
{
	ExitSccAux();
	RestoreSerialInputBuffer();
	if (sccOldBconmap > 0)
	{
		Bconmap((short)sccOldBconmap);
	}
}

bool Scc_IsConnected(void)
{
	return true;
	/*
		DCD seems to be a bit problematic on Falcon... Or just my falcon...
		The DCD pin implementation in hardware differs between Falcon and mega STE.
		The mega STE have a pull upp resistor connected to the pin and the falcon do not.
		If this is the reason why I can detect DCD on mega STE and not on falcon is worth investigating. 
	*/
}

int Scc_TransmitByte(unsigned char byte)
{
	return AuxTransmitByte(byte, Scc_IsConnected);
}

int Scc_ReceiveByte(void)
{
	return AuxReceiveByte(Scc_IsConnected);
}

const char*	Scc_DeviceName(void)
{
	return "SCC serial device.\r\n";
}


comm SccCom =
{
	1,
	Scc_IsMyDevice,
	Scc_DeviceName,
	Scc_Init,
	Scc_Exit,
	Scc_TransmitByte,
	Scc_ReceiveByte,
	Scc_IsConnected,
	SetCtrlCFlag,
};

comm* GetSccSerial(void)
{
	return &SccCom;
}

comm* GetPluginComm()
{
	return (comm*)Cookie_SDBG;
}

comm* InitComm(const char *comString)
{
	const char* defaultString = "AUX";
	const char* sdbgString = "SDBG";
	if (comString == 0 || comString[0] == 0)
	{
		// Set AUX to be default if none other is specified.
		comString = defaultString;
		if (Cookie_SDBG != 0)
		{
			// Set SDBG to be default if plugin cookie exists.
			comString = sdbgString;
		}
	}
	if (Mfp_IsMyDevice(comString))
	{
		// All computers with modem1 connected to MFP.
		return GetMfpSerial();
	}
	else if (Scc_IsMyDevice(comString))
	{
		// Falcon, modem2 dsub9 port connected to SCC.
		return GetSccSerial();
	}
	else
	{
		comm* com = GetPluginComm();
		if (com != 0 && com->APIversion == 1 && com->IsMyDevice(comString))
		{
			// Plugin is compatible
			return com;
		}
	}
	return 0;
}
