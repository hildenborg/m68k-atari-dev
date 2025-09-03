/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#include "comm.h"
#include "bios_calls.h"
#include "server.h"

extern short StringCompare(const char* str_a, const char* str_b);

volatile short CtrlC_enable;
volatile unsigned char Mfp_ActiveEdgeRegister __attribute__((aligned(2))) = 0;

void InitMfpCom1(_CommException CommException);
void ExitMfpCom1(void);

bool Mfp_IsMyDevice(char *comString)
{
	return (StringCompare("COM1", comString) > 0);
}


int Mfp_Init(char *comString, _CommException CommException)
{
	InitMfpCom1(CommException);

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
	ExitMfpCom1();
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


void Mfp_EnableCtrlC(bool enable)
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
	com->EnableCtrlC = Mfp_EnableCtrlC;
}

void CreateSccSerial(comm* com)
{

}

int InitComm(const char *comString, comm* com)
{
	const char* defaultString = "COM1";
	if (comString == 0 || comString[0] == 0)
	{
		comString = defaultString;
	}
	if (StringCompare("COM1", comString) > 0)
	{
		if ((Cookie_MCH >> 16) == 3)
		{
			// Falcon
			CreateSccSerial(com);
			return 0;
		}
		else
		{
			// All computers with modem1 connected to MFP.
			CreateMfpSerial(com);
			return 0;
		}
	}
	return -1;
}
