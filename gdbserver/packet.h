/*
	Copyright (C) 2026 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#ifndef PACKET_DEFINED
#define PACKET_DEFINED

#include <stdbool.h>

#define PACKET_SIZE 0x3ff

extern bool	noAckMode;

char* GetInpacketPtr(short offset);
short GetInPacketLength(void);
char* GetOutpacketPtr(short offset);
short GetOutPacketLength(void);
void ClearOutPacket(void);

void ReceivePacket(void);
void TransmitPacket(bool skipAck);

void WriteChar(char c);
short GetHexString(short offset, char** strOut);
void WriteError(int e);
void WriteByte(unsigned char c);
void WriteLong(unsigned int val);
void WriteString(const char* str);
void WriteNameAndLong(const char* name, unsigned int v);
short ReadNumber(short inOff, unsigned int* result);
short GetAddressAndLength(short offset, bool write, unsigned char** addr, unsigned int* len);
short GetFileArgs(short cmdEnd, char* argv[]);
void GetInferiorNameAndArgs(short vNameEnd, char* inferior_filename, char* inferior_cmdline);
void WriteVariable(int val);
void WriteFileResponse(int result, int ioErrno, const char* attachment);
void WriteStop(int si_signo, int si_code, bool start_break);
void ReadRegisters(void);
void WriteRegisters(void);
void WriteRegister(void);
void ReadRegister(void);
void WriteTargetXML(short vNameEnd);

#define WriteOK() WriteString("OK")
#define WriteEQ() WriteString("EQ")
#define WriteNE() WriteString("NE")

#endif // PACKET_DEFINED
