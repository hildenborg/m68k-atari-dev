/*
	This is not gdbserver.
	It is something that partly works similarily to gdbserver.
	The usage is similar enough to warrant the name gdbserver.

	We do not use any standard libraries or c runtimes in this code.
	Only pure atari code here to limit the effect this server has on the system.
	And also limit the size. 
	A smaller gdbserver allows us to debug larger programs.
*/

#include "server.h"

int main(int argc, char** argv)
{
	return ServerMain(argc, argv);
}