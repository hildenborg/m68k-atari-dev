/*
	Copyright (C) 2026 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#include "packet.h"
#include "comm.h"
#include "hex.h"
#include "log.h"
#include "clib.h"
#include "exceptions.h"
#include "gdb_defines.h"
#include "target_xml.h"
#include "cookies.h"
#include "inferior.h"
#include "context.h"
#include "critical.h"

/*
	Max inPacket and outPacket size must be the same, 
	and must also be set as size-1 in serverFeatures.
*/
short	inPacketLength = 0;
char	inPacket[PACKET_SIZE + 1] __attribute__((aligned(2)));
short	outPacketLength = 0;
char	outPacket[PACKET_SIZE + 1] __attribute__((aligned(2)));

bool	noAckMode = false;				// gdb QStartNoAckMode

// server
int CheckServerQuitKey(void);
extern unsigned int numOfCpuRegisters;

char* GetInpacketPtr(short offset)
{
	return &inPacket[offset];
}

short GetInPacketLength(void)
{
	return inPacketLength;
}

char* GetOutpacketPtr(short offset)
{
	return &outPacket[offset];
}

short GetOutPacketLength(void)
{
	return outPacketLength;
}

void ClearOutPacket(void)
{
	outPacketLength = 0;
}

void ReceivePacket(void)
{
	DbgRemOut("ReceivePacket: \r\n");
	int c;
	bool waitForPacket = true;

	while (waitForPacket)
	{
		unsigned char sum = 0;
		inPacketLength = 0;
		DbgRemOut("\tWaiting...\r\n");
		
		// Wait for connection.
		while (!comDev->IsConnected())
		{
			// Check keypress
			// If we do not have any connection yet, then we cannot have any running inferiors, which means that we cannot be in supervisor mode.
			// Meaning, to peek the keycode, we need to enter supervisor mode.
			if (CheckServerQuitKey() < 0)
			{
				DbgOut("\r\n\tKill Server!\r\n");
				inPacketLength = 1;
				inPacket[0] = 0x1a;		// Ctrl-Z
				return;
			}
		}
		DbgRemOut("\tGot connection...\r\n");

		// Wait for packet start
		while ((c = GetByte()) != '$')
		{
			if (c == COMM_ERR_DISCONNECTED)
			{
				DbgRemOut("\r\n\tConnection dropped!\r\n");
				inPacketLength = 1;
				inPacket[0] = (unsigned char)'k';
				return;
			}
			else if (c == COMM_ERR_KILLED)
			{
				DbgRemOut("\r\n\tKill Server!\r\n");
				inPacketLength = 1;
				inPacket[0] = 0x1a;		// Ctrl-Z
				return;
			}
		}
		
		DbgRemOut("\tGot beginning of packet.\r\n");
		// Fetch payload
		bool escaped = false;
		bool error = false;
		while (!error) 
		{
			c = GetByte();
			if (c == COMM_ERR_DISCONNECTED)
			{
				DbgRemOut("\r\n\tConnection dropped!\r\n");
				inPacketLength = 1;
				inPacket[0] = (unsigned char)'k';
				return;
			}
			else if (c == COMM_ERR_KILLED)
			{
				DbgRemOut("\r\n\tKill Server!\r\n");
				inPacketLength = 1;
				inPacket[0] = 0x1a;		// Ctrl-Z
				return;
			}
			if (escaped)
			{
				sum += (unsigned char)c;
				c ^= 0x20;
				escaped = false;
				inPacket[inPacketLength++] = (unsigned char)c;
			}
			else
			{
				if (c == '$')
				{
					// Error, wait for new packet.
					error = true;
					break;
				}
				else if (c == '#')
				{
					// End of packet.
					break;
				}
				else if (c == 0x7d)
				{
					escaped = true;
				}
				else
				{
					inPacket[inPacketLength++] = (unsigned char)c;
				}
				sum += (unsigned char)c;
			}
		}
		if (error)
		{
			DbgRemOut("\r\n\tError in packet, retrying.\r\n\t");
			continue;
		}
		inPacket[inPacketLength] = 0;
		DbgRemOut("\tGot end of packet.\r\n\t");
		DbgRemOut(inPacket);
		// Fetch checksum
		char csum[2];
		csum[0] = (char)GetByte();
		csum[1] = (char)GetByte();
		if (!noAckMode)
		{
			unsigned char psum = HexToByte(csum);
			waitForPacket = sum != psum;
			PutByte(waitForPacket ? '-' : '+');
			if (!waitForPacket)
			{
				DbgRemOut("\r\n\tPacket checksum OK.\r\n");
			}
			else
			{
				DbgRemOut("\r\n\tError - Packet checksum not OK!\r\n");
				DbgRemOutVal("Checksum", sum);
			}
		}
		else
		{
			waitForPacket = false;
		}
		if (!comDev->IsConnected())
		{
			DbgRemOut("\r\n\tConnection dropped!\r\n");
			inPacketLength = 1;
			inPacket[0] = 'k';	// Kill inferior
			return;
		}
	}
}

void TransmitPacket(bool skipAck)
{
	DbgRemOut("TransmitPacket:\r\n\t");
	outPacket[outPacketLength] = 0;
	DbgRemOut(outPacket);

	if (!comDev->IsConnected())
	{
		// Connection dropped...
		DbgRemOut("\r\n\tConnection dropped!\r\n");
		return;
	}
	
	unsigned char sum = 0;
	// Send encoded
	PutByte('$');	// Packets always start with $
	for (int i = 0; i < outPacketLength; ++i)
	{
		char c = outPacket[i];
		if (c == '$' || c == '#' || c == '*' || c == 0x7d)
		{
			PutByte(0x7d);
			sum += 0x7d;
			c ^= 0x20;
		}
		PutByte(c);
		sum += (unsigned char)c;
	}
	// End with checksum
	PutByte('#');
	PutByte(NibbleToHex(sum >> 4));
	PutByte(NibbleToHex(sum));

	if (!noAckMode && !skipAck)
	{
		// Wait for ack
		int ack = GetByte();
		if (ack == '+')
		{
			DbgRemOut("\r\n\tAck OK.\r\n");
		}
		else if (ack < 0)
		{
			// Connection dropped...
			DbgRemOut("\r\n\tConnection dropped!\r\n");
		}
		else
		{
			DbgRemOut("\r\n\tError - Ack not OK!\r\n");
		}
	}
	else
	{
		DbgRemOut("\r\n");
	}
}

void WriteChar(char c)
{
	outPacket[outPacketLength++] = c;
}

short GetHexString(short offset, char** strOut)
{
	char* str = *strOut;
	short i = offset;
	while (i < inPacketLength && inPacket[i] != ';')
	{
		*str++ = (char)HexToByte(inPacket + i);
		i += 2;
	}
	if (inPacket[i] == ';')
	{
		++i;
	}
	*strOut = str;
	return i;
}

void WriteError(int e)
{
	WriteChar('E');
	WriteByte((unsigned char)e);
}

void WriteByte(unsigned char c)
{
	WriteChar(NibbleToHex(c >> 4));
	WriteChar(NibbleToHex(c));
}

// Endian aware
void WriteLong(unsigned int val)
{
	for (int i = 28; i >= 0; i -= 4)
	{
		WriteChar(NibbleToHex(val >> i));
	}
}

void WriteString(const char* str)
{
	char c;
	while ((c = *str++) != 0)
	{
		WriteChar(c);
	}
}

void WriteNameAndLong(const char* name, unsigned int v)
{
	WriteString(name);
	WriteChar('=');
	WriteLong(v);
}

short ReadNumber(short inOff, unsigned int* result)
{
	unsigned int v = 0;
	short offset = inOff;

	while (offset < inPacketLength && inPacket[offset] != '=')
	{
		char c = inPacket[offset];
		if (c == '=' || c == ',' || c == ':') { break; }
		v = (v << 4) | (unsigned int)HexToNibble(c);
		++offset;
	}
	*result = v;

	return (inOff == offset) ? -1 : offset;
}

short GetAddressAndLength(short offset, bool write, unsigned char** addr, unsigned int* len)
{
	unsigned int add;
	if ((offset = ReadNumber(offset, &add)) > 0)
	{
		*addr = (unsigned char*)add;
		if (inPacket[offset] == ',')
		{
			if ((offset = ReadNumber(offset + 1, len)) > 0)
			{
				if (write)
				{
					if (inPacket[offset] == ':')
					{
						++offset;
						if ((offset + (short)(*len * 2)) == inPacketLength)
						{
							return offset;
						}
					}
				}
				else
				{
					return offset;
				}
			}
		}
	}
	return -1;
}

short GetFileArgs(short cmdEnd, char* argv[])
{
	// Find operand and arguments and end all with 0
	char *ptr = inPacket + cmdEnd;
	char *ptrend = inPacket + inPacketLength;
	short argc = 0;
	do
	{
		argv[argc++] = ptr;
		while (*ptr != 0 && *ptr != ',' && *ptr != ':')
		{
			++ptr;
		}
		if (*ptr != 0)
		{
			*ptr++ = 0;
		}
	} while (argc < 4 && ptr != ptrend);
	return argc;
}

void GetInferiorNameAndArgs(short vNameEnd, char* inferior_filename, char* inferior_cmdline)
{
	char* strOut = inferior_filename;
	inferior_cmdline[0] = 0;	// length
	inferior_cmdline[1] = 0;
	short args = GetHexString(vNameEnd, &strOut);	// Will not replace old filename if no new exists
	if (args > vNameEnd)
	{
		strOut = inferior_cmdline + 1;
		while (args < inPacketLength)
		{
			args = GetHexString(args, &strOut);
			*strOut++ = ' ';
		}
		if (inferior_cmdline[1] != 0)
		{
			strOut[-1] = 0;
			inferior_cmdline[0] = (char)strlen(&inferior_cmdline[1]);		
		}
	}
}

void WriteVariable(int val)
{
	if (val < 0)
	{
		WriteChar('-');
		val = -val;
	}
	int v = val >> 4;
	int l = 0;
	while (v != 0)
	{
		l += 4;
		v >>= 4;
	}

	for (int i = l; i >= 0; i -= 4)
	{
		WriteChar(NibbleToHex(val >> i));
	}
}

void WriteFileResponse(int result, int ioErrno, const char* attachment)
{
	WriteChar('F');
	WriteVariable(result);
	if (result < 0)
	{
		WriteChar(',');
		WriteVariable(ioErrno);
	}
	if (attachment != 0)
	{
		WriteChar(';');
		for (int i = 0; i < result; ++i)
		{
			WriteChar(attachment[i]);
			//WriteByte(attachment[i]);
		}
	}
}

void WriteStop(int si_signo, int si_code, bool start_break)
{
	if (si_signo == GDB_SIGTRAP && si_code == TRAP_BRKPT)
	{
		// Software breakpoint hit.
		WriteChar('T');
		WriteByte((unsigned char)si_signo);
		unsigned int* ptr = (unsigned int*)GetRegisters();
		if (!start_break)
		{
			WriteString("swbreak:;");
		}
		// Report fp, sp, sr, pc
		for (unsigned char i = 14; i <= 17; ++i)
		{
			WriteByte(i);
			WriteChar(':');
			WriteLong(ptr[i]);
			WriteChar(';');
		}
	}
	else
	{
		WriteChar('S');
		WriteByte((unsigned char)si_signo);
	}
}

void ReadRegisters(void)
{
	unsigned int* ptr = (unsigned int*)GetRegisters();
	size_t num = 18;
	if ((Cookie_FPU & (0x1f << 16)) != 0)
	{
		num += (8*3) + 3;
	}

	for (size_t i = 0; i < num; ++i)
	{
		WriteLong(ptr[i]);
	}
}

void WriteRegisters(void)
{
	if (GetInPacketLength() != (sizeof(ExceptionRegisters) * 2) + 1)
	{
		WriteError(1);
	}
	else
	{
		unsigned int* ptr = (unsigned int*)GetRegisters();
		char* buf = GetInpacketPtr(1);
		size_t num = 18;
		if ((Cookie_FPU & (0x1f << 16)) != 0)
		{
			num += (8*3) + 3;
		}
		for (size_t i = 0; i < num; ++i)
		{
			ptr[i] = HexToLong(buf);
			buf += 8;
		}
		WriteOK();
	}
}

void WriteRegister(void)
{
	unsigned int* rptr = (unsigned int*)GetRegisters();
	short offset;
	unsigned int idx;
	if ((offset = ReadNumber(1, &idx)) > 0)
	{
		if (idx < numOfCpuRegisters)
		{
			char *inptr = GetInpacketPtr(offset);
			if (*inptr == '=')
			{
				++inptr;
				if (idx < 18)
				{
					rptr[idx] = HexToLong(inptr);
				}
				else if (idx >= (18 + 8))
				{
					idx += 16;
					rptr[idx] = HexToLong(inptr);
				}
				else
				{
					idx = 18 + ((idx - 18) * 3);
					rptr[idx] = HexToLong(inptr);
					rptr[idx + 1] = HexToLong(inptr + 8);
					rptr[idx + 2] = HexToLong(inptr + 16);
				}
			}
		}
		else
		{
			// Just ignore.
		}
	}

	WriteOK();
}

void ReadRegister(void)
{
	unsigned int* rptr = (unsigned int*)GetRegisters();
	unsigned int idx;
	if (ReadNumber(1, &idx) > 0)
	{
		if (idx < numOfCpuRegisters)
		{
			if (idx < 18)
			{
				WriteLong(rptr[idx]);
			}
			else if (idx >= (18 + 8))
			{
				idx += 16;
				WriteLong(rptr[idx]);
			}
			else
			{
				idx = 18 + ((idx - 18) * 3);
				WriteLong(rptr[idx]);
				WriteLong(rptr[idx + 1]);
				WriteLong(rptr[idx + 2]);
			}
		}
		/*
		else
		{
			WriteLong(0x0);
		}
		*/
	}
}

void WriteTargetXML(short vNameEnd)
{
	unsigned char* addr;
	unsigned int len;
	if (GetAddressAndLength(vNameEnd, false, &addr, &len) > 0)
	{
		unsigned int offset = (unsigned int)addr;
		
		const char* xmls[5];
		unsigned int xml_len = GetTargetXml(xmls);
		
		unsigned int maxRead = (PACKET_SIZE - 20);	// max packet size - some room for response
		if ((offset + len) > xml_len)
		{
			// This transfer ends the xml transfer
			WriteChar('l');
			if (offset >= xml_len || len == 0)
			{
				return;
			}
			len = xml_len - offset;
		}
		else
		{
			// Too much for one packet, only send a part of it.
			WriteChar('m');
			len = maxRead;
		}

		const char* xml;
		int ix = 0;
		while ((xml = xmls[ix++]) != 0 && len != 0)
		{
			unsigned int slen = strlen(xml);
			if (offset >= slen)
			{
				offset -= slen;
			}
			else
			{
				char c;
				unsigned int i = offset;
				while ((c = xml[i]) != 0 && len != 0)
				{
					WriteChar(c);
					++i;
					--len;
				}
				offset = 0;
			}
		}
	}
}

void WriteOffsets(void)
{
	if (inferiorBasePage != NULL)	// Return empty if no inferior
	{
		unsigned int textoffset = (unsigned int)(inferiorBasePage->p_tbase);
		unsigned int dataoffset = (unsigned int)(inferiorBasePage->p_dbase);
		WriteNameAndLong("TextSeg", textoffset);
		WriteChar(';');
		if (inferior_is_mintelf)
		{
			// m68k-atari-mintelf toolchain produces data symbols that use the
			// data segment as base.
			WriteNameAndLong("DataSeg", dataoffset);
		}
		else
		{
			// m68k-atari-elf toolchain produces data symbols that uses the same 
			// base as the text segment.
			WriteNameAndLong("DataSeg", textoffset);
		}
	}
}

void WriteMemory(bool isSupervisorMode)
{
	unsigned char* addr;
	unsigned int len;
	short offset = GetAddressAndLength(1, true, &addr, &len);
	if (isSupervisorMode && offset > 0)
	{
		char* ptr = GetInpacketPtr(offset);
		for (unsigned int i = 0; i < len; ++i)
		{
			unsigned char* infAddr = InferiorContextMemoryAddress(addr + i);
			ExceptionSafeMemoryWrite(infAddr, HexToByte(ptr));
			ptr += 2;
		}
	}
	else
	{
		WriteError(1);
	}
}

void ReadMemory(bool isSupervisorMode)
{
	unsigned char* addr;
	unsigned int len;
	short offset = GetAddressAndLength(1, false, &addr, &len);
	if (isSupervisorMode && offset > 0)
	{
		for (unsigned int i = 0; i < len; ++i)
		{
			unsigned char* infAddr = InferiorContextMemoryAddress(addr + i);
			unsigned char membyte;
			ExceptionSafeMemoryRead(infAddr, &membyte);
			WriteByte(membyte);
		}
	}
	else
	{
		WriteError(1);
	}
}
