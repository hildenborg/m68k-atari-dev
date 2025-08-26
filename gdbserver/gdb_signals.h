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

#endif // GDB_SIGNALS_DEFINED

