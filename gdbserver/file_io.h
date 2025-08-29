/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#ifndef FILE_IO_DEFINED
#define FILE_IO_DEFINED

typedef struct _vfile_stat {
	unsigned int st_dev;      /* device */
	unsigned int st_ino;      /* inode */
	unsigned int st_mode;     /* protection */
	unsigned int st_nlink;    /* number of hard links */
	unsigned int st_uid;      /* user ID of owner */
	unsigned int st_gid;      /* group ID of owner */
	unsigned int st_rdev;     /* device type (if inode device) */
	unsigned int _st_size;    /* high long word of st_size, always 0 */
	unsigned int st_size;     /* total size, in bytes */
	unsigned int _st_blksize; /* high long word of st_blksize, always 0 */
	unsigned int st_blksize;  /* blocksize for filesystem I/O */
	unsigned int _st_blocks;  /* high long word of st_blocks, always 0 */
	unsigned int st_blocks;   /* number of blocks allocated */
	unsigned int st_atime;    /* time of last access */
	unsigned int st_mtime;    /* time of last modification */
	unsigned int st_ctime;    /* time of last change */
} vfile_stat;

void InitFileIO(void);
void ExitFileIO(void);

int VfileOpen(const char *bios_path, int flags, int *ioErrno);

int VfileClose(int fd, int *ioErrno);

int VfileDelete(const char *bios_path, int *ioErrno);

int VfileWrite(int fd, const void *buf, int offset, int nbytes, int *ioErrno);

int VfileRead(int fd, void *buf, int offset, int nbytes, int *ioErrno);

int VfileFstat(int fd, vfile_stat* stat, int *ioErrno);

int VfileStat(const char *bios_path, vfile_stat* stat, int *ioErrno);

#endif // FILE_IO_DEFINED
