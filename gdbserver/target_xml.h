/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#ifndef TARGET_XML_DEFINED
#define TARGET_XML_DEFINED

#ifdef __cplusplus
extern "C" {
#endif

// Writes len characters of the target description, from offset, with WriteChar. Returns its total length.
unsigned int WriteTargetXmlPart(unsigned int offset, unsigned int len);

#ifdef __cplusplus
}
#endif

#endif // TARGET_XML_DEFINED
