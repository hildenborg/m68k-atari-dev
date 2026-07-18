#include <stdbool.h>
#include <stddef.h>
#include "inferior.h"
#include "gdb_defines.h"
#include "server.h"
#include "bios_calls.h"
#include "exceptions.h"
#include "log.h"

#define MINTELF_RESERVED 0x454c4628
#define M68K_ATARI_ELF_RESERVED 0x68e1f001	// 68e1f = haxxor 68elf. 001 is version number.

struct BasePage*	inferiorBasePage = NULL;		// basepage for debugged exectable
InferiorState		inferiorState = NOT_LOADED;		// To know if we have an inferior and if we have started it or not.
bool				inferior_is_mintelf = false;	// If set then the inferior loaded is of mintelf executable format.

bool inferiorTerminatedByServer = false;
unsigned short* __start_Breakpoint = NULL;			// Only set during startup of inferior, and used to break at __start.
char	inferior_filename[MAX_PATH_LEN] __attribute__((aligned(2)));	// The filename of the inferior being debugged. Can be empty if nothing is loaded.
char	inferior_cmdline[MAX_PATH_LEN] __attribute__((aligned(2)));		// Command line args to debugged inferior.
char	inferior_workpath[MAX_PATH_LEN] __attribute__((aligned(2)));	// The work path of the inferior being debugged. Can be empty if nothing is loaded.

bool CheckIfMintElf(const char* fileName)
{
	unsigned int reserved_long_word;
	int fd = Fopen(fileName, VFILE_O_RDONLY);
	if (fd < 0)
	{
		// File not found, but let the caller handle that problem.
		return false;
	}
	Fseek(18,(unsigned short)fd, 0);	// position of reserved long word
	Fread((unsigned short)fd, 4, &reserved_long_word);
	Fclose((unsigned short)fd);
	return reserved_long_word == MINTELF_RESERVED;
}

int LoadInferior(const char* fileName, const char* cmdLine, const char* environment)
{
	DbgOut("LoadInferior: ");
	int loadres = -1;
	// Check that we doesn't already have an inferior loaded.
	if (inferiorState == NOT_LOADED)
	{
		inferior_is_mintelf = CheckIfMintElf(fileName);

		// Load inferior
		loadres = Pexec(PE_LOAD, fileName, cmdLine, environment);
		if (loadres > 0)
		{
			inferiorBasePage = (struct BasePage*)loadres;
			// Set one time breakpoint at _start.
			__start_Breakpoint = (unsigned short*)(inferiorBasePage->p_tbase);
			if (InsertMemoryBreakpoint(__start_Breakpoint) != 0)
			{
				__start_Breakpoint = NULL;
			}
			inferiorState = LOADED;
			DbgOut("LOADED\r\n");
		}
		else
		{
			inferiorBasePage = NULL;
			inferiorState = NOT_LOADED;
			DbgOut("NOT_LOADED\r\n");
		}
	}
	else
	{
			DbgOut("Error - not unloaded!\r\n");
	}
	return loadres;
}

bool RunInferior(int* return_code)
{
	DbgOut("RunInferior: Started\r\n");
	// Run inferior while saving and restoring various system properties.
	inferiorState = RUNNING;
	inferiorTerminatedByServer = false;
//	*return_code = Pexec(PE_GOTHENFREE, 0, (const char*)inferiorBasePage, 0); // Requires GEMDOS 0.19 or above.
	*return_code = Pexec(PE_GO, 0, (const char*)inferiorBasePage, 0);
	Mfree(inferiorBasePage->p_env);
	Mfree(inferiorBasePage);
	// Returning from a terminated inferior
	inferiorState = NOT_LOADED;
	inferiorBasePage = NULL;	// inferior have been unloaded.
	DbgOut("RunInferior: Terminated\r\n");
	return inferiorTerminatedByServer;
}

void __attribute__ ((noreturn)) TerminateInferior(int si_signo)
{
	DbgOut("TerminateInferior: Terminating...\r\n");
	inferiorTerminatedByServer = true;
	Pterm(si_signo == GDB_SIGABRT ? -1 : -32);
	// The code execution will not continue here. It will continue after Pexec() in RunInferior().
}

bool IsInferiorStartBreak(int si_signo, int si_code)
{
	return (si_signo == GDB_SIGTRAP && 
		si_code == TRAP_BRKPT &&
		(unsigned int)__start_Breakpoint == GetRegisters()->pc);
}

void ClearInferiorStartBreak(void)
{
	if (RemoveMemoryBreakpoint(__start_Breakpoint) == 0)
	{
		__start_Breakpoint = NULL;
	}
}

