/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#include "target_xml.h"
#include "clib.h"

extern unsigned int Cookie_FPU;

char target_xml_header[] =
	"<?xml version=\"1.0\"?>\n"
	"<!DOCTYPE target SYSTEM \"gdb-target.dtd\">\n"
	"<target version=\"1.0\">\n"
	;

char target_xml_cpu_feature[] =
	"  <feature name=\"org.gnu.gdb.m68k.core\">\n"
	"    <reg name=\"d0\" bitsize=\"32\"/>\n"
	"    <reg name=\"d1\" bitsize=\"32\"/>\n"
	"    <reg name=\"d2\" bitsize=\"32\"/>\n"
	"    <reg name=\"d3\" bitsize=\"32\"/>\n"
	"    <reg name=\"d4\" bitsize=\"32\"/>\n"
	"    <reg name=\"d5\" bitsize=\"32\"/>\n"
	"    <reg name=\"d6\" bitsize=\"32\"/>\n"
	"    <reg name=\"d7\" bitsize=\"32\"/>\n"
	"    <reg name=\"a0\" bitsize=\"32\" type=\"data_ptr\"/>\n"
	"    <reg name=\"a1\" bitsize=\"32\" type=\"data_ptr\"/>\n"
	"    <reg name=\"a2\" bitsize=\"32\" type=\"data_ptr\"/>\n"
	"    <reg name=\"a3\" bitsize=\"32\" type=\"data_ptr\"/>\n"
	"    <reg name=\"a4\" bitsize=\"32\" type=\"data_ptr\"/>\n"
	"    <reg name=\"a5\" bitsize=\"32\" type=\"data_ptr\"/>\n"
	"    <reg name=\"fp\" bitsize=\"32\" type=\"data_ptr\"/>\n"
	"    <reg name=\"sp\" bitsize=\"32\" type=\"data_ptr\"/>\n"
	"    <reg name=\"ps\" bitsize=\"32\"/>\n"
	"    <reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\"/>\n"
	"  </feature>\n"
	;

char target_xml_fpu_feature[] =
	"  <feature name=\"org.gnu.gdb.coldfire.fp\">\n"
	"    <reg name=\"fp0\" bitsize=\"96\" type=\"float\" group=\"float\"/>\n"
	"    <reg name=\"fp1\" bitsize=\"96\" type=\"float\" group=\"float\"/>\n"
	"    <reg name=\"fp2\" bitsize=\"96\" type=\"float\" group=\"float\"/>\n"
	"    <reg name=\"fp3\" bitsize=\"96\" type=\"float\" group=\"float\"/>\n"
	"    <reg name=\"fp4\" bitsize=\"96\" type=\"float\" group=\"float\"/>\n"
	"    <reg name=\"fp5\" bitsize=\"96\" type=\"float\" group=\"float\"/>\n"
	"    <reg name=\"fp6\" bitsize=\"96\" type=\"float\" group=\"float\"/>\n"
	"    <reg name=\"fp7\" bitsize=\"96\" type=\"float\" group=\"float\"/>\n"
	"    <reg name=\"fpcontrol\" bitsize=\"32\" group=\"float\"/>\n"
	"    <reg name=\"fpstatus\" bitsize=\"32\" group=\"float\"/>\n"
	"    <reg name=\"fpiaddr\" bitsize=\"32\" type=\"code_ptr\" group=\"float\"/>\n"
	"  </feature>\n"
	;

char target_xml_footer[] =
	"</target>\n"
	;

unsigned int GetTargetXml(const char* parts[])
{
	unsigned int length = strlen(target_xml_header) + strlen(target_xml_footer) + strlen(target_xml_cpu_feature);
	parts[0] = target_xml_header;
	parts[1] = target_xml_cpu_feature;

	if ((Cookie_FPU & (0x1f << 16)) != 0)
	{
		parts[2] = target_xml_fpu_feature;
		length += strlen(target_xml_fpu_feature);
		parts[3] = target_xml_footer;
		parts[4] = 0;
	}
	else
	{
		parts[2] = target_xml_footer;
		parts[3] = 0;
	}
	return length;
}