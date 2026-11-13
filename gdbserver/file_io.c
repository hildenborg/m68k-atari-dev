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
// GEMDOS file attributes (FA_*) for each open handle, captured with Fattrib when the file is opened.
// Needed by fstat, as GEMDOS cannot get the attributes from a file handle.
unsigned char fd_attribs[NUM_HANDLES];

void VfileFixPath(char *fileName)
{
	char c;
	while ((c = *fileName) != 0)
	{
		if (c == '/')
		{
			*fileName = '\\';
		}
		++fileName;
	}
}

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

int AddHandle(int fd, unsigned char attrib)
{
	for (int i = 0; i < NUM_HANDLES; ++i)
	{
		if (fd_handles[i] == -1)
		{
			fd_handles[i] = fd;
			fd_attribs[i] = attrib;
			return 0;
		}
	}
	return -1;
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

// Returns index in fd_handles, or -1 if not found.
int FindHandle(int fd)
{
	for (int i = 0; i < NUM_HANDLES; ++i)
	{
		if (fd_handles[i] == fd)
		{
			return i;
		}
	}
	return -1;
}

int IsHandle(int fd)
{
	return FindHandle(fd) >= 0 ? 0 : -1;
}

int gem_to_errno(int gemError)
{
    int err = VFILE_ERRNO_EACCES;
	if (gemError == -33 || gemError == -34)
	{
		err = VFILE_ERRNO_ENOENT;
	}
    return err;
}

int VfileOpen(const char *fileName, int flags, int *ioErrno)
{
	int fd = -1;
	unsigned short bios_mode = (unsigned short)(flags & 0x3);
	bool create = (flags & VFILE_O_CREAT) != 0;
	bool append = (flags & VFILE_O_APPEND) != 0;
	bool excl = (flags & VFILE_O_EXCL) != 0;
	bool trunc = (flags & VFILE_O_TRUNC) != 0;

	if (!trunc)
	{
		fd = Fopen(fileName, bios_mode);
	}
	if (fd < 0 && (create || trunc))
	{
		unsigned short bios_attrib = 0;
		fd = Fcreate(fileName, bios_attrib);
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
		int attrib = Fattrib(fileName, 0, 0);
		if (attrib < 0)
		{
			attrib = 0;
		}
		if (AddHandle(fd, (unsigned char)attrib) != 0)
		{
			Fclose((unsigned short)fd);
			*ioErrno = VFILE_ERRNO_EACCES;
			return -1;
		}
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

int VfileDelete(const char *fileName, int *ioErrno)
{
	int result = Fdelete(fileName);
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

static void FillStat(vfile_stat* stat, int attrib, unsigned int size)
{
	memset(stat, 0, sizeof(vfile_stat));
	stat->st_mode = VFILE_S_IRUSR | VFILE_S_IRGRP | VFILE_S_IROTH;
	stat->st_mode |= (attrib & FA_DIR) ? VFILE_S_IFDIR : VFILE_S_IFREG;
	if ((attrib & FA_READONLY) == 0)
	{
		stat->st_mode |= VFILE_S_IWUSR | VFILE_S_IWGRP | VFILE_S_IWOTH;
	}
	stat->st_size = size;
	// st_uid, st_gid have no meaning for the st, so we ignore them.
}

int VfileFstat(int fd, vfile_stat* stat, int *ioErrno)
{
	int i = FindHandle(fd);
	if (i < 0)
	{
		*ioErrno = VFILE_ERRNO_EBADF;
		return -1;
	}
	// Get file size by seeking to the end, then restore the file position.
	int pos = Fseek(0, (unsigned short)fd, 1);
	int size = (pos < 0) ? pos : Fseek(0, (unsigned short)fd, 2);
	if (size < 0)
	{
		*ioErrno = gem_to_errno(size);
		return -1;
	}
	Fseek(pos, (unsigned short)fd, 0);
	FillStat(stat, fd_attribs[i], (unsigned int)size);
	return sizeof(vfile_stat);
}

/*
	Note! The caller uses the same buffer (inPacket) for both fileName and stat,
	so fileName must not be used after FillStat has been called.
*/
int VfileStat(const char *fileName, vfile_stat* stat, int *ioErrno)
{
	int attrib = Fattrib(fileName, 0, 0);
	if (attrib < 0)
	{
		*ioErrno = gem_to_errno(attrib);
		return -1;
	}
	int size = 0;
	if ((attrib & FA_DIR) == 0)
	{
		// Directories can't be opened, but files must be opened to get the size.
		int fd = Fopen(fileName, 0);
		if (fd < 0)
		{
			*ioErrno = gem_to_errno(fd);
			return -1;
		}
		size = Fseek(0, (unsigned short)fd, 2);
		Fclose((unsigned short)fd);
		if (size < 0)
		{
			*ioErrno = gem_to_errno(size);
			return -1;
		}
	}
	FillStat(stat, attrib, (unsigned int)size);
	return sizeof(vfile_stat);
}
