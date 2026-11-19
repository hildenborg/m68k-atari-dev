/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#include "target_xml.h"
#include "packet.h"
#include "cookies.h"

/*
	The target description is generated from tables instead of stored as text, to save memory.
	A register group is either one register (count 0), or count registers named prefix0, prefix1...
	attribs is the rest of the reg element after 'bitsize="'.
*/
typedef struct
{
	const char* name;
	short count;
	const char* attribs;
} RegGroup;

static const char int32[] = "32\"";
static const char dataPtr[] = "32\" type=\"data_ptr\"";
static const char codePtr[] = "32\" type=\"code_ptr\"";
static const char float32[] = "32\" group=\"float\"";

static const RegGroup cpuRegs[] =
{
	{ "d", 8, int32 },
	{ "a", 6, dataPtr },
	{ "fp", 0, dataPtr },
	{ "sp", 0, dataPtr },
	{ "ps", 0, int32 },
	{ "pc", 0, codePtr },
	{ 0, 0, 0 }
};

static const RegGroup fpuRegs[] =
{
	{ "fp", 8, "96\" type=\"float\" group=\"float\"" },
	{ "fpcontrol", 0, float32 },
	{ "fpstatus", 0, float32 },
	{ "fpiaddr", 0, "32\" type=\"code_ptr\" group=\"float\"" },
	{ 0, 0, 0 }
};

// State for writing one part of the xml.
static unsigned int xmlSkip;	// Characters left to skip before the requested offset.
static unsigned int xmlRoom;	// Characters left to write.
static unsigned int xmlLength;	// Total length of the xml so far.

static void Put(const char* str)
{
	for (; *str != 0; ++str)
	{
		++xmlLength;
		if (xmlSkip != 0)
		{
			--xmlSkip;
		}
		else if (xmlRoom != 0)
		{
			WriteChar(*str);
			--xmlRoom;
		}
	}
}

static void PutFeature(const char* featureName, const RegGroup* group)
{
	Put("<feature name=\"");
	Put(featureName);
	Put("\">");
	for (; group->name != 0; ++group)
	{
		char digit[2] = { '0', 0 };
		short i = 0;
		do
		{
			Put("<reg name=\"");
			Put(group->name);
			if (group->count != 0)
			{
				Put(digit);
				++digit[0];
			}
			Put("\" bitsize=\"");
			Put(group->attribs);
			Put("/>");
		} while (++i < group->count);
	}
	Put("</feature>");
}

unsigned int WriteTargetXmlPart(unsigned int offset, unsigned int len)
{
	xmlSkip = offset;
	xmlRoom = len;
	xmlLength = 0;
	Put("<?xml version=\"1.0\"?><!DOCTYPE target SYSTEM \"gdb-target.dtd\"><target version=\"1.0\">");
	PutFeature("org.gnu.gdb.m68k.core", cpuRegs);
	if (Fpu_Coprocessor != 0)
	{
		PutFeature("org.gnu.gdb.coldfire.fp", fpuRegs);
	}
	Put("</target>");
	return xmlLength;
}
