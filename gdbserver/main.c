/*
	This is not gdbserver.
	It is something that partly works similarily to gdbserver.
	The usage is similar enough to warrant the name gdbserver.
*/

#include "server.h"

// We use _HeapSize to define that we do not need all of the memory (_HEAP_SIZE = 0 set by linker if we do not specify it ourselves).
unsigned int _HEAP_SIZE = 1024;	// We really do not need much heap, better save it for the inferior.

int main(int argc, char** argv)
{
	return ServerMain(argc, argv);
}