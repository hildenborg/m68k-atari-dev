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

typedef bool	(*_IsMyDevice)(char *comString);
typedef int 	(*_Init)(char *comString, _CommException CommException);
typedef void	(*_Exit)(void);
typedef int		(*_TransmitByte)(unsigned char byte);
typedef int		(*_ReceiveByte)(void);
typedef bool	(*_IsConnected)(void);
typedef void	(*_EnableCtrlC)(bool);

typedef struct _comm
{
	_IsMyDevice IsMyDevice;
	_Init Init;					// Always called in supervisor mode
	_Exit Exit;					// Always called in supervisor mode
	_TransmitByte TransmitByte;
	_ReceiveByte ReceiveByte;
	_IsConnected IsConnected;
	_EnableCtrlC EnableCtrlC;
} comm;

int InitComm(const char *comString, comm* com);

#endif // COMM_DEFINED
