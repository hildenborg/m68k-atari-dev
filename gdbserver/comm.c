/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#include "comm.h"
#include "bios_calls.h"
#include "server.h"

extern short StringCompare(const char* str_a, const char* str_b);

volatile short CtrlC_enable;
volatile unsigned char Mfp_ActiveEdgeRegister;
volatile unsigned char Scc_StatusRegister;

void InitMfpAux(_CommException CommException);
void ExitMfpAux(void);

void InitSccAux(_CommException CommException);
void ExitSccAux(void);


bool Mfp_IsMyDevice(char *comString)
{
	if ((Cookie_MCH >> 16) < 3 && StringCompare("AUX", comString) >= 0)
	{
		return true;
	}
	return false;
}


int Mfp_Init(char *comString, _CommException CommException)
{
	InitMfpAux(CommException);

	/*
		Don't do any serial configuration.
		It is better to leave that to external apps like serial.cpx
	*/
	// Set serial conf
	// Rsconf(BAUD_9600, FLOW_HARD, RS_CLK16 | RS_1STOP | RS_8BITS, RS_INQUIRE, RS_INQUIRE, RS_INQUIRE);
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


void CreateMfpSerial(comm* com)
{
	com->IsMyDevice = Mfp_IsMyDevice;
	com->Init = Mfp_Init;
	com->Exit = Mfp_Exit;
	com->TransmitByte = Mfp_TransmitByte;
	com->ReceiveByte = Mfp_ReceiveByte;
	com->IsConnected = Mfp_IsConnected;
	com->EnableCtrlC = SetCtrlCFlag;
}

bool Scc_IsMyDevice(char *comString)
{
	if ((Cookie_MCH >> 16) == 3 && StringCompare("AUX", comString) >= 0)
	{
		return true;
	}
	return false;
}

int Scc_Init(char *comString, _CommException CommException)
{
	InitSccAux(CommException);

	/*
		Don't do any serial configuration.
		It is better to leave that to external apps like serial.cpx
	*/
	// Set serial conf
	// Rsconf(BAUD_9600, FLOW_HARD, RS_CLK16 | RS_1STOP | RS_8BITS, RS_INQUIRE, RS_INQUIRE, RS_INQUIRE);
	// Empty serial buffer
	while (Bconstat(DEV_AUX) != 0)
	{
		Bconin(DEV_AUX);
	}
	Scc_StatusRegister = 0xff;

//	Ongibit(GI_DTR);
	return 0;
}

void Scc_Exit(void)
{
	
	ExitSccAux();
}


bool Scc_IsConnected(void)
{
	return (Scc_StatusRegister & 0x08) != 0;
}


int Scc_TransmitByte(unsigned char byte)
{
	if (Bcostat(DEV_AUX) == 0)
	{
		if (!Scc_IsConnected())
		{
			return COMM_ERR_DISCONNECTED;
		}
		return COMM_ERR_NOT_READY;
	}
	Bconout(DEV_AUX, (unsigned short)byte);
	return 0;
}


int Scc_ReceiveByte(void)
{
	if (Bconstat(DEV_AUX) == 0)
	{
		if (!Scc_IsConnected())
		{
			return COMM_ERR_DISCONNECTED;
		}
		return COMM_ERR_NOT_READY;
	}
	return Bconin(DEV_AUX) & 0xff;
}

void CreateSccSerial(comm* com)
{
	com->IsMyDevice = Scc_IsMyDevice;
	com->Init = Scc_Init;
	com->Exit = Scc_Exit;
	com->TransmitByte = Scc_TransmitByte;
	com->ReceiveByte = Scc_ReceiveByte;
	com->IsConnected = Scc_IsConnected;
	com->EnableCtrlC = SetCtrlCFlag;
}

int InitComm(const char *comString, comm* com)
{
	const char* defaultString = "AUX";
	if (comString == 0 || comString[0] == 0)
	{
		comString = defaultString;
	}
	if (Mfp_IsMyDevice(comString))
	{
		// All computers with modem1 connected to MFP.
		CreateMfpSerial(com);
		return 0;
	}
	else if (Scc_IsMyDevice(comString))
	{
		// Falcon, modem1 connected to SCC.
		/*
			There is some confusion in documentation regarding com ports for the Falcon.
			Some name the lan port as modem1 and serial port as modem2, while
			others name lan port as lan port and serial port as modem1...
			This software choose to follow the serial.cpx naming.
		*/
		CreateSccSerial(com);
		return 0;
	}
	return -1;
}
