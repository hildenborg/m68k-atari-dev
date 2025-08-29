/*
	Copyright (C) 2025 Mikael Hildenborg
	SPDX-License-Identifier: MIT
*/

#ifndef GDB_SIGNALS_DEFINED
#define GDB_SIGNALS_DEFINED

/*
    This contains signal defenitions to be compatible with GDB.
    https://sourceware.org/gdb/current/onlinedocs/gdb.html/Signals.html
    https://sourceware.org/git/?p=binutils-gdb.git;a=blob_plain;f=include/gdb/signals.def;hb=HEAD
*/

#define GDB_SIGHUP      1
#define GDB_SIGINT      2
#define GDB_SIGQUIT     3
#define GDB_SIGILL      4
#define GDB_SIGTRAP     5
#define GDB_SIGABRT     6
#define GDB_SIGEMT      7
#define GDB_SIGFPE      8
#define GDB_SIGKILL     9
#define GDB_SIGBUS      10
#define GDB_SIGSEGV     11
#define GDB_SIGSYS      12
#define GDB_SIGPIPE     13
#define GDB_SIGALRM     14
#define GDB_SIGTERM     15
#define GDB_SIGUSR1     30

#ifndef BUS_ADRALN
#define BUS_ADRALN	1
#endif

#ifndef BUS_ADRERR
#define BUS_ADRERR	2
#endif

#ifndef ILL_ILLOPC
#define ILL_ILLOPC	3
#endif

#ifndef FPE_INTDIV
#define FPE_INTDIV	4
#endif

#ifndef FPE_INTOVF
#define FPE_INTOVF	5
#endif

#ifndef ILL_PRVOPC
#define ILL_PRVOPC	6
#endif

#ifndef TRAP_TRACE
#define TRAP_TRACE	7
#endif

#ifndef BUS_OBJERR
#define BUS_OBJERR	8
#endif

#ifndef TRAP_BRKPT
#define TRAP_BRKPT	9
#endif

#ifndef FPE_FLTRES
#define FPE_FLTRES  10
#endif

#ifndef FPE_FLTDIV
#define FPE_FLTDIV  11
#endif

#ifndef FPE_FLTUND
#define FPE_FLTUND  12
#endif

#ifndef FPE_FLTOVF
#define FPE_FLTOVF  13
#endif

#ifndef FPE_FLTINV
#define FPE_FLTINV  14
#endif

// Responses for file operations
#define VFILE_ERRNO_EPERM           1
#define VFILE_ERRNO_ENOENT          2
#define VFILE_ERRNO_EINTR           4
#define VFILE_ERRNO_EBADF           9
#define VFILE_ERRNO_EACCES         13
#define VFILE_ERRNO_EFAULT         14
#define VFILE_ERRNO_EBUSY          16
#define VFILE_ERRNO_EEXIST         17
#define VFILE_ERRNO_ENODEV         19
#define VFILE_ERRNO_ENOTDIR        20
#define VFILE_ERRNO_EISDIR         21
#define VFILE_ERRNO_EINVAL         22
#define VFILE_ERRNO_ENFILE         23
#define VFILE_ERRNO_EMFILE         24
#define VFILE_ERRNO_EFBIG          27
#define VFILE_ERRNO_ENOSPC         28
#define VFILE_ERRNO_ESPIPE         29
#define VFILE_ERRNO_EROFS          30
#define VFILE_ERRNO_ENAMETOOLONG   91
#define VFILE_ERRNO_EUNKNOWN       9999

#define VFILE_O_RDONLY        0x0
#define VFILE_O_WRONLY        0x1
#define VFILE_O_RDWR          0x2
#define VFILE_O_APPEND        0x8
#define VFILE_O_CREAT       0x200
#define VFILE_O_TRUNC       0x400
#define VFILE_O_EXCL        0x800

#define VFILE_S_IFREG       0100000
#define VFILE_S_IFDIR        040000
#define VFILE_S_IRUSR          0400
#define VFILE_S_IWUSR          0200
#define VFILE_S_IXUSR          0100
#define VFILE_S_IRGRP           040
#define VFILE_S_IWGRP           020
#define VFILE_S_IXGRP           010
#define VFILE_S_IROTH            04
#define VFILE_S_IWOTH            02
#define VFILE_S_IXOTH            01

#endif // GDB_SIGNALS_DEFINED

