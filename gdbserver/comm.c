/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#include "comm.h"
#include "bios_calls.h"
#include "server.h"
#include "clib.h"
#include "cookies.h"

int CheckServerQuitKey(void);

volatile short CtrlC_enable;
volatile unsigned char Mfp_ActiveEdgeRegister;
volatile unsigned char Scc_StatusRegister;
volatile unsigned short sccTmpData;

void InitMfpAux(_CommException CommException);
void ExitMfpAux(void);

void InitSccAux(_CommException CommException);
void ExitSccAux(void);

int SccBcostat(void);
int SccBconout(void);
int SccBconin(void);
int SccBconstat(void);

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


int Mfp_Init(const char *comString, _CommException CommException)
{
	InitMfpAux(CommException);

	// Set serial conf
	 Rsconf(BAUD_9600, FLOW_HARD, RS_CLK16 | RS_1STOP | RS_8BITS, RS_INQUIRE, RS_INQUIRE, RS_INQUIRE);
	// Empty serial buffer
	while (Bconstat(DEV_AUX) != 0)
	{
		Bconin(DEV_AUX);
	}
	
	// Set DTR to ON
	Ongibit(GI_DTR);

	return 0;
}


void Mfp_Exit(void)
{
	// Set DTR to OFF
	Offgibit(GI_DTR);
	ExitMfpAux();
}


bool Mfp_IsConnected(void)
{
	return (Mfp_ActiveEdgeRegister & 0x02) == 0;
}


int Mfp_TransmitByte(unsigned char byte)
{
	if (Bcostat(DEV_AUX) == 0)
	{
		if (!Mfp_IsConnected())
		{
			return COMM_ERR_DISCONNECTED;
		}
		return COMM_ERR_NOT_READY;
	}
	Bconout(DEV_AUX, (unsigned short)byte);
	return 0;
}


int Mfp_ReceiveByte(void)
{
	if (Bconstat(DEV_AUX) == 0)
	{
		if (!Mfp_IsConnected())
		{
			return COMM_ERR_DISCONNECTED;
		}
		return COMM_ERR_NOT_READY;
	}
	return Bconin(DEV_AUX) & 0xff;
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

int Scc_Init(const char *comString, _CommException CommException)
{
	InitSccAux(CommException);
	
	return 0;
}

void Scc_Exit(void)
{
	ExitSccAux();
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
	//return (Scc_StatusRegister & 0x08) != 0;
}


int Scc_TransmitByte(unsigned char byte)
{
	if (SccBcostat() == 0)
	{
		if (!Scc_IsConnected())
		{
			return COMM_ERR_DISCONNECTED;
		}
		return COMM_ERR_NOT_READY;
	}
	sccTmpData = (unsigned short)byte;
	if (Supexec(SccBconout) < 0)
	{
		return COMM_ERR_NOT_READY;
	}
	return 0;
}

int Scc_ReceiveByte(void)
{
	if (SccBconstat() == 0)
	{
		if (!Scc_IsConnected())
		{
			return COMM_ERR_DISCONNECTED;
		}
		return COMM_ERR_NOT_READY;
	}
	Supexec(SccBconin);
	return sccTmpData;
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
