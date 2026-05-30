/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#ifndef COMM_DEFINED
#define COMM_DEFINED

#include <stdbool.h>

#define COMM_ERR_DISCONNECTED	-1
#define COMM_ERR_KILLED			-2
#define COMM_ERR_NOT_READY		-3


typedef void	(*_CommException)(void);

typedef bool	(*_IsMyDevice)(const char *comString);
typedef int 	(*_Init)(const char *comString, _CommException CommException);
typedef void	(*_Exit)(void);
typedef int		(*_TransmitByte)(unsigned char byte);
typedef int		(*_ReceiveByte)(void);
typedef bool	(*_IsConnected)(void);
typedef void	(*_EnableCtrlC)(bool);
typedef const char*	(*_DeviceName)(void);

typedef struct _comm
{
	unsigned int APIversion;	// 1 for now	
	_IsMyDevice IsMyDevice;
	_DeviceName DeviceName;
	_Init Init;					// Always called in supervisor mode
	_Exit Exit;					// Always called in supervisor mode
	_TransmitByte TransmitByte;
	_ReceiveByte ReceiveByte;
	_IsConnected IsConnected;
	_EnableCtrlC EnableCtrlC;
} comm;

extern comm* comDev;

int GetByte(void);
void PutByte(char ch);

comm* InitComm(const char *comString);

#endif // COMM_DEFINED
