/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#include <stdbool.h>
#include "gdb_defines.h"
#include "bios_calls.h"
#include "file_io.h"
#include "clib.h"

#define NUM_HANDLES 8
int fd_handles[NUM_HANDLES];

void InitFileIO(void)
{
	for (int i = 0; i < NUM_HANDLES; ++i)
	{
		fd_handles[i] = -1;
	}
}

void ExitFileIO(void)
{
	for (int i = 0; i < NUM_HANDLES; ++i)
	{
		if (fd_handles[i] != -1)
		{
			Fclose((unsigned short)fd_handles[i]);
			fd_handles[i] = -1;
		}
	}
}

// Silent fail
void AddHandle(int fd)
{
	for (int i = 0; i < NUM_HANDLES; ++i)
	{
		if (fd_handles[i] == -1)
		{
			fd_handles[i] = fd;
			break;
		}
	}
}

// Silent fail
void RemoveHandle(int fd)
{
	for (int i = 0; i < NUM_HANDLES; ++i)
	{
		if (fd_handles[i] == fd)
		{
			fd_handles[i] = -1;
			break;
		}
	}
}

int IsHandle(int fd)
{
	for (int i = 0; i < NUM_HANDLES; ++i)
	{
		if (fd_handles[i] == fd)
		{
			return 0;
		}
	}
	return -1;
}

int gem_to_errno(int gemError)
{
    int err = VFILE_ERRNO_EACCES;
	if (gemError == -33 || gemError == -34)
	{
		err = VFILE_ERRNO_EACCES;
	}
    return err;
}

int VfileOpen(const char *bios_path, int flags, int *ioErrno)
{
	int fd = -1;
	unsigned short bios_mode = (unsigned short)(flags & 0x3);
	bool create = (flags & VFILE_O_CREAT) != 0;
	bool append = (flags & VFILE_O_APPEND) != 0;
	bool excl = (flags & VFILE_O_EXCL) != 0;
	bool trunc = (flags & VFILE_O_TRUNC) != 0;

	if (!trunc)
	{
		fd = Fopen(bios_path, bios_mode);
	}
	if (fd < 0 && (create || trunc))
	{
		unsigned short bios_attrib = 0;
		fd = Fcreate(bios_path, bios_attrib);
	}
	else if (create && excl)
	{
		// We explicitly specified that file must be created, and it already existed, so error!
		// Close file.
		Fclose((unsigned short)fd);
        *ioErrno = VFILE_ERRNO_EACCES;
		return -1;
	}

	if (fd >= 0 && append)
	{
		// Seek to end.
		int new_file_pos = Fseek(0, (unsigned short)fd, 2);
		if (new_file_pos < 0)
		{
			// Close file.
			Fclose((unsigned short)fd);
			*ioErrno = gem_to_errno(new_file_pos);
			return -1;
		}
	}

	if (fd < 0)
	{
		*ioErrno = gem_to_errno(fd);
		fd = -1;
	}
	else
	{
		AddHandle(fd);
	}
	return fd;
}

int VfileClose(int fd, int *ioErrno)
{
	int result = Fclose((unsigned short)fd);
	if (result < 0)
	{
		*ioErrno = gem_to_errno(result);
		return -1;
	}
	else
	{
		RemoveHandle(fd);
	}
	return 0;
}

int VfileDelete(const char *bios_path, int *ioErrno)
{
	int result = Fdelete(bios_path);
	if (result < 0)
	{
		*ioErrno = gem_to_errno(result);
		return -1;
	}
	return 0;
}

int VfileWrite(int fd, const void *buf, int offset, int nbytes, int *ioErrno)
{
	int numWritten = -1;
	if (IsHandle(fd) == 0)
	{
		int new_file_pos = Fseek(offset, (unsigned short)fd, 0);
		if (new_file_pos < 0)
		{
			*ioErrno = gem_to_errno(new_file_pos);
			return -1;
		}
		numWritten = Fwrite((unsigned short)fd, nbytes, buf);
	}
	if (numWritten < 0)
	{
		*ioErrno = gem_to_errno(numWritten);
		return -1;
	}
	return numWritten;
}

int VfileRead(int fd, void *buf, int offset, int nbytes, int *ioErrno)
{
	int numRead = -1;
	if (IsHandle(fd) == 0)
	{
		int new_file_pos = Fseek(offset, (unsigned short)fd, 0);
		if (new_file_pos < 0)
		{
			*ioErrno = gem_to_errno(new_file_pos);
			return -1;
		}
		numRead = Fread((unsigned short)fd, nbytes, buf);
	}
	if (numRead < 0)
	{
		*ioErrno = gem_to_errno(numRead);
		return -1;
	}
	return numRead;
}

int VfileFstat(int fd, vfile_stat* stat, int *ioErrno)
{
	memset(stat, 0, sizeof(vfile_stat));
	struct DTA *dta = (struct DTA *)-1;
	if (IsHandle(fd) == 0)
	{
		stat->st_mode = VFILE_S_IRUSR | VFILE_S_IRGRP | VFILE_S_IROTH;
		// files
		dta = Fgetdta((unsigned int)fd);
		if ((int)dta >= 0)
		{
			// Attribs
			stat->st_mode |= (dta->d_attrib & FA_DIR) ? VFILE_S_IFDIR : VFILE_S_IFREG;
			if ((dta->d_attrib & FA_READONLY) != 0)
			{
				stat->st_mode |= VFILE_S_IWUSR | VFILE_S_IWGRP | VFILE_S_IWOTH;
			}
			stat->st_size = dta->d_length;
			// st_uid, st_gid have no meaning for the st, so we ignore them.
		}
	}
	else
	{
		*ioErrno = VFILE_ERRNO_EINVAL;
		return -1;
	}
	if ((int)dta < 0)
	{
		*ioErrno = gem_to_errno((int)dta);
		return -1;
	}
	return sizeof(vfile_stat);
}

int VfileStat(const char *bios_path, vfile_stat* stat, int *ioErrno)
{
	int handle = Fopen(bios_path, 0);
	if (handle > 0)
	{
		int err = VfileFstat(handle, stat, ioErrno);
		Fclose(handle);
		handle = err;
	}
	if (handle < 0)
	{
		*ioErrno = gem_to_errno(handle);
		return -1;
	}
	return sizeof(vfile_stat);
}
