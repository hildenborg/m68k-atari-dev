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
#include "server.h"
#include "bios_calls.h"

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
		bool overflow = false;
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
				if (inPacketLength < PACKET_SIZE)
				{
					inPacket[inPacketLength++] = (unsigned char)c;
				}
				else
				{
					overflow = true;
				}
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
				else if (inPacketLength < PACKET_SIZE)
				{
					inPacket[inPacketLength++] = (unsigned char)c;
				}
				else
				{
					overflow = true;
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
		if (overflow && !waitForPacket)
		{
			/*
				Packet was larger than we can handle. It has been acked (a nak would only make gdb
				resend the same packet), so turn it into an empty packet, which is answered with an
				empty reply meaning "not supported".
			*/
			DbgRemOut("\r\n\tError - Packet too large, ignored.\r\n");
			inPacketLength = 0;
			inPacket[0] = 0;
		}
	}
}

// Max number of times a packet is resent when gdb answers with a nak ('-').
#define MAX_TRANSMIT_RETRIES 10

// Sends outPacket as "$<escaped data>#<checksum>".
static void SendPacketData(void)
{
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
	
	for (int retries = 0; ; ++retries)
	{
		SendPacketData();

		if (noAckMode || skipAck)
		{
			DbgRemOut("\r\n");
			return;
		}

		// Wait for ack
		int ack = GetByte();
		if (ack == '+')
		{
			DbgRemOut("\r\n\tAck OK.\r\n");
			return;
		}
		else if (ack < 0)
		{
			// Connection dropped...
			DbgRemOut("\r\n\tConnection dropped!\r\n");
			return;
		}
		else if (ack == '-' && retries < MAX_TRANSMIT_RETRIES)
		{
			// gdb got a corrupted packet and requests it to be sent again.
			DbgRemOut("\r\n\tNak, resending.\r\n");
		}
		else
		{
			DbgRemOut("\r\n\tError - Ack not OK!\r\n");
			return;
		}
	}
}

void WriteChar(char c)
{
	// Safety net only. Callers must limit what they write to fit in a packet.
	if (outPacketLength < PACKET_SIZE)
	{
		outPacket[outPacketLength++] = c;
	}
}

short GetHexString(short offset, char** strOut, char* strEnd)
{
	char* str = *strOut;
	short i = offset;
	while (i + 1 < inPacketLength && inPacket[i] != ';' && inPacket[i + 1] != ';')
	{
		// Keep parsing even if out of space, so the returned offset is still correct.
		if (str < strEnd)
		{
			*str++ = (char)HexToByte(inPacket + i);
		}
		i += 2;
	}
	// Skip any malformed trailing half byte.
	while (i < inPacketLength && inPacket[i] != ';')
	{
		++i;
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
						// Data must be exactly len bytes (2 hex digits each). Avoid len * 2, as it may overflow.
						unsigned int hexDigits = (unsigned int)(inPacketLength - offset);
						if ((hexDigits & 1) == 0 && *len == hexDigits / 2)
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

short GetFileArgs(short cmdEnd, char* argv[MAX_FILE_ARGS])
{
	/*
		Find operand and arguments and end all with 0.
		Scanning stops when the last argument has been found, as it may be binary data
		(pwrite) that can contain any byte value, including ',' and ':'.
		The last argument is terminated by the 0 that ends inPacket.
	*/
	char *ptr = inPacket + cmdEnd;
	char *ptrend = inPacket + inPacketLength;
	short argc = 0;
	argv[argc++] = ptr;
	while (argc < MAX_FILE_ARGS && ptr < ptrend)
	{
		if (*ptr == ',' || *ptr == ':')
		{
			*ptr = 0;
			argv[argc++] = ptr + 1;
		}
		++ptr;
	}
	return argc;
}

void GetInferiorNameAndArgs(short vNameEnd, char* inferior_filename, char* inferior_cmdline)
{
	char* strOut = inferior_filename;
	inferior_cmdline[0] = 0;	// length
	inferior_cmdline[1] = 0;
	// Will not replace old filename if no new exists
	short args = GetHexString(vNameEnd, &strOut, inferior_filename + MAX_PATH_LEN - 1);
	if (strOut != inferior_filename)
	{
		*strOut = 0;
	}
	if (args > vNameEnd)
	{
		char* cmdStart = inferior_cmdline + 1;
		char* cmdEnd = cmdStart + MAX_CMDLINE_LEN;
		strOut = cmdStart;
		while (args < inPacketLength)
		{
			args = GetHexString(args, &strOut, cmdEnd);
			if (strOut < cmdEnd)
			{
				*strOut++ = ' ';
			}
		}
		// Remove trailing space
		if (strOut > cmdStart && strOut[-1] == ' ')
		{
			--strOut;
		}
		*strOut = 0;
		inferior_cmdline[0] = (char)(strOut - cmdStart);
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

void WriteStop(int si_signo, int si_code, bool gdb_break)
{
	if (si_signo == GDB_SIGTRAP && si_code == TRAP_BRKPT)
	{
		// Software breakpoint hit.
		WriteChar('T');
		WriteByte((unsigned char)si_signo);
		unsigned int* ptr = (unsigned int*)GetRegisters();
		if (gdb_break)
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

size_t GetNumRegisterLongs(void)
{
	size_t num = 18;
	if (Fpu_Coprocessor != 0)
	{
		num += (8*3) + 3;
	}
	return num;
}

void ReadRegisters(void)
{
	unsigned int* ptr = (unsigned int*)GetRegisters();
	size_t num = GetNumRegisterLongs();

	for (size_t i = 0; i < num; ++i)
	{
		WriteLong(ptr[i]);
	}
}

void WriteRegisters(void)
{
	size_t num = GetNumRegisterLongs();
	if (GetInPacketLength() != (num * 8 + 1))
	{
		WriteError(1);
	}
	else
	{
		unsigned int* ptr = (unsigned int*)GetRegisters();
		char* buf = GetInpacketPtr(1);
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
			// FPU data registers are 96 bits, all others 32 bits.
			short hexDigits = (idx >= 18 && idx < (18 + 8)) ? 24 : 8;
			if (*inptr == '=' && (inPacketLength - (offset + 1)) >= hexDigits)
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
			else
			{
				// Malformed packet, or too little register data.
				WriteError(1);
				return;
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
		if (len > maxRead)
		{
			len = maxRead;
		}
		if (offset >= xml_len)
		{
			// Nothing more to send.
			WriteChar('l');
			return;
		}
		if (len >= xml_len - offset)
		{
			// This transfer ends the xml transfer
			WriteChar('l');
			len = xml_len - offset;
		}
		else
		{
			// More data remains after this part.
			WriteChar('m');
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

// Arguments to the memory access functions, which may run through Supexec.
static unsigned char*	memAddr;
static unsigned int		memLen;
static char*			memHex;
static bool				memInferiorContext;

static int WriteMemory_super(void)
{
	char* ptr = memHex;
	for (unsigned int i = 0; i < memLen; ++i)
	{
		unsigned char* a = memAddr + i;
		if (memInferiorContext)
		{
			a = InferiorContextMemoryAddress(a);
		}
		if (ExceptionSafeMemoryWrite(a, HexToByte(ptr)) != 0)
		{
			return (int)i;	// Bus or address error.
		}
		ptr += 2;
	}
	return (int)memLen;
}

static int ReadMemory_super(void)
{
	for (unsigned int i = 0; i < memLen; ++i)
	{
		unsigned char* a = memAddr + i;
		if (memInferiorContext)
		{
			a = InferiorContextMemoryAddress(a);
		}
		unsigned char membyte;
		if (ExceptionSafeMemoryRead(a, &membyte) != 0)
		{
			return (int)i;	// Bus or address error.
		}
		WriteByte(membyte);
	}
	return (int)memLen;
}

/*
	Memory is accessed in supervisor mode, as the exception safe access needs it,
	and so that system variables and hardware registers can be read.
	When no inferior is stopped (isSupervisorMode is false) the server runs in user mode,
	so Supexec is used. The server context is then the current one, so there is no
	inferior context to redirect addresses to.
	Returns the number of bytes accessed before any bus or address error.
*/
static unsigned int AccessMemory(int (*func)(void), bool isSupervisorMode)
{
	memInferiorContext = isSupervisorMode;
	if (isSupervisorMode)
	{
		return (unsigned int)func();
	}
	return (unsigned int)Supexec(func);
}

void WriteMemory(bool isSupervisorMode)
{
	unsigned char* addr;
	unsigned int len;
	short offset = GetAddressAndLength(1, true, &addr, &len);
	if (offset > 0)
	{
		memAddr = addr;
		memLen = len;
		memHex = GetInpacketPtr(offset);
		if (AccessMemory(WriteMemory_super, isSupervisorMode) == len)
		{
			WriteOK();
		}
		else
		{
			WriteError(1);	// M has no reply for a partial write.
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
	if (offset > 0)
	{
		// Each byte takes two hex digits. gdb accepts a shorter reply than requested.
		if (len > PACKET_SIZE / 2)
		{
			len = PACKET_SIZE / 2;
		}
		memAddr = addr;
		memLen = len;
		// A read that fails part way replies with the bytes read so far, which gdb accepts.
		// If not even the first byte can be read, it is an error.
		if (AccessMemory(ReadMemory_super, isSupervisorMode) == 0 && len != 0)
		{
			WriteError(1);
		}
	}
	else
	{
		WriteError(1);
	}
}
