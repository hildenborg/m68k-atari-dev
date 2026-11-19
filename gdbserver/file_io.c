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
// GEMDOS file attributes (FA_*) for each open handle, looked up when the file is opened.
// Needed by fstat, as GEMDOS cannot get the attributes from a file handle.
unsigned char fd_attribs[NUM_HANDLES];

/*
	Converts a GEMDOS (DOS format) date and time to seconds since 1970-01-01.
	GEMDOS keeps local time without any time zone, so it is treated as UTC.
	Valid for all dates GEMDOS can store (1980 - 2107). Returns 0 for an invalid date.

	Only uses 16x16 bit multiplications, as 32x32 bit multiplication on 68000
	would need __mulsi3 from libgcc, which we don't link with.
*/
static unsigned int DosToUnixTime(unsigned short date, unsigned short time)
{
	static const unsigned short daysBeforeMonth[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
	unsigned short year = date >> 9;			// Years since 1980.
	unsigned short month = (date >> 5) & 0xf;	// 1 - 12
	unsigned short day = date & 0x1f;			// 1 - 31
	if (month < 1 || month > 12 || day < 1)
	{
		return 0;
	}
	// Every 4th year from 1980 is a leap year, except 2100 (year 120).
	bool leap = (year & 3) == 0 && year != 120;
	unsigned short leapDaysBefore = (year + 3) >> 2;	// Leap years from 1980 up to, not including, year.
	if (year > 120)
	{
		--leapDaysBefore;
	}
	// 1970-01-01 to 1980-01-01 is 3652 days. Max result is below 65536 (year 2149).
	unsigned short days = 3652 + (unsigned short)(year * 365) + leapDaysBefore + daysBeforeMonth[month - 1] + (day - 1);
	if (leap && month > 2)
	{
		++days;
	}
	unsigned short hours = time >> 11;
	unsigned short minutes = (time >> 5) & 0x3f;
	unsigned short seconds = (time & 0x1f) << 1;
	// days * 86400 = (days * 675) << 7, keeping the multiplication 16x16 bits.
	unsigned int secs = ((unsigned int)days * (unsigned short)675) << 7;
	secs += (unsigned int)hours * (unsigned short)3600;
	secs += (unsigned int)(unsigned short)(minutes * 60) + seconds;
	return secs;
}

/*
	Gets attributes, size and modification time of a file or directory, without opening it.
	Returns 0, or a negative GEMDOS error code.

	Fsfirst writes its result to the DTA of the current process.
	When called from an exception, the current process is the inferior,
	so a temporary DTA is used and the original is always restored.
*/
static int GetFileInfo(const char* fileName, unsigned char* attrib, unsigned int* size, unsigned int* mtime)
{
	struct DTA dta;
	struct DTA* oldDta = Fgetdta();
	Fsetdta(&dta);
	int result = Fsfirst(fileName, FA_READONLY | FA_HIDDEN | FA_SYSTEM | FA_DIR | FA_ARCHIVE);
	Fsetdta(oldDta);
	if (result == 0)
	{
		*attrib = dta.d_attrib;
		*size = dta.d_length;
		*mtime = DosToUnixTime(dta.d_date, dta.d_time);
	}
	return result;
}

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

	if (create && excl)
	{
		// The file must not exist, not even when O_TRUNC is also given.
		// Checked before Fcreate, as Fcreate would truncate an existing file.
		unsigned char attrib;
		unsigned int size;
		unsigned int mtime;
		if (GetFileInfo(fileName, &attrib, &size, &mtime) == 0)
		{
			*ioErrno = VFILE_ERRNO_EEXIST;
			return -1;
		}
		fd = Fcreate(fileName, 0);
	}
	else if (trunc)
	{
		if (!create)
		{
			// Without O_CREAT the file must already exist.
			// Checked before Fcreate, as Fcreate would create a missing file.
			unsigned char attrib;
			unsigned int size;
			unsigned int mtime;
			int result = GetFileInfo(fileName, &attrib, &size, &mtime);
			if (result != 0)
			{
				*ioErrno = gem_to_errno(result);
				return -1;
			}
		}
		fd = Fcreate(fileName, 0);
	}
	else
	{
		fd = Fopen(fileName, bios_mode);
		if (fd < 0 && create)
		{
			fd = Fcreate(fileName, 0);
		}
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
		unsigned char attrib = 0;
		unsigned int size;
		unsigned int mtime;
		GetFileInfo(fileName, &attrib, &size, &mtime);	// Keeps attrib as 0 on failure.
		if (AddHandle(fd, attrib) != 0)
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

static void FillStat(vfile_stat* stat, unsigned char attrib, unsigned int size, unsigned int mtime)
{
	memset(stat, 0, sizeof(vfile_stat));
	stat->st_mode = VFILE_S_IRUSR | VFILE_S_IRGRP | VFILE_S_IROTH;
	stat->st_mode |= (attrib & FA_DIR) ? VFILE_S_IFDIR : VFILE_S_IFREG;
	if ((attrib & FA_READONLY) == 0)
	{
		stat->st_mode |= VFILE_S_IWUSR | VFILE_S_IWGRP | VFILE_S_IWOTH;
	}
	stat->st_size = size;
	// GEMDOS only keeps one time stamp per file.
	stat->st_atime = mtime;
	stat->st_mtime = mtime;
	stat->st_ctime = mtime;
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
	// Get the size by seeking to the end, then restore the file position.
	// This also includes anything written through this handle.
	int pos = Fseek(0, (unsigned short)fd, 1);
	int size = (pos < 0) ? pos : Fseek(0, (unsigned short)fd, 2);
	if (size < 0)
	{
		*ioErrno = gem_to_errno(size);
		return -1;
	}
	Fseek(pos, (unsigned short)fd, 0);
	struct DOSTIME dt;
	unsigned int mtime = 0;
	if (Fdatime(&dt, (unsigned short)fd, 0) == 0)
	{
		mtime = DosToUnixTime(dt.date, dt.time);
	}
	FillStat(stat, fd_attribs[i], (unsigned int)size, mtime);
	return sizeof(vfile_stat);
}

/*
	Note! The caller uses the same buffer (inPacket) for both fileName and stat,
	so fileName must not be used after FillStat has been called.
*/
int VfileStat(const char *fileName, vfile_stat* stat, int *ioErrno)
{
	unsigned char attrib = 0;
	unsigned int size = 0;
	unsigned int mtime = 0;
	int result = GetFileInfo(fileName, &attrib, &size, &mtime);
	if (result < 0)
	{
		*ioErrno = gem_to_errno(result);
		return -1;
	}
	FillStat(stat, attrib, size, mtime);
	return sizeof(vfile_stat);
}
