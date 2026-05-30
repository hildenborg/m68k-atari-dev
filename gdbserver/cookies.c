/*
	Copyright (C) 2026 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#include "cookies.h"
#include "log.h"

unsigned int Cookie_CPU = 0;
unsigned int Cookie_VDO = 0;
unsigned int Cookie_FPU = 0;
unsigned int Cookie_MCH = 0;
unsigned int Cookie_SDBG = 0;

#pragma GCC diagnostic push
// Remove out of bounds warning, as the cookie code will trigger false warnings.
#pragma GCC diagnostic ignored "-Warray-bounds="

/*
	This function must be executed in supervisor mode or it will crash.
*/
int GetCookies(void)
{
	DbgOut("Using system cookies:\r\n");
	
	struct cookie
	{
		unsigned int cookie;
		unsigned int value;
	};
	#define COOKIE_NAME(a,b,c,d) ((a<<24)|(b<<16)|(c<<8)|(d))
	struct cookie* _p_cookies = ((struct cookie**)0x5a0)[0];
	if (_p_cookies != 0)
	{
		while (_p_cookies->cookie != 0)
		{
			switch(_p_cookies->cookie)
			{
				case COOKIE_NAME('_', 'C', 'P', 'U'):
					Cookie_CPU = _p_cookies->value;
					DbgOutVal("\t_CPU", Cookie_CPU);
					break;
				case COOKIE_NAME('_', 'V', 'D', 'O'):
					Cookie_VDO = _p_cookies->value;
					DbgOutVal("\t_VDO", Cookie_VDO);
					break;
				case COOKIE_NAME('_', 'F', 'P', 'U'):
					Cookie_FPU = _p_cookies->value;
					DbgOutVal("\t_FPU", Cookie_FPU);
					break;
				case COOKIE_NAME('_', 'M', 'C', 'H'):
					Cookie_MCH = _p_cookies->value;
					DbgOutVal("\t_MCH", Cookie_MCH);
					break;
				case COOKIE_NAME('S', 'D', 'B', 'G'):
					Cookie_SDBG = _p_cookies->value;
					DbgOutVal("\tSDBG", Cookie_SDBG);
					break;
			}
			++_p_cookies;
		}
	}
	#undef COOKIE_NAME
	return 0;
}
#pragma GCC diagnostic pop
