## Requirements
This example needs you to connect an Atari computer to your PC using a USB to RS232 cable.  
You also need to have "gdbsrv.ttp" on your Atari computer.  

## Usage
Start "gdbsrv.ttp" on your Atari and enter: "--multi" in the prompt.  
Observe that we don't enter any filename to debug.  
On your PC, start the script: "vscode.sh" which will set some variables and start VsCode.  
In VsCode, press: "CTRL - Shift - B" to build and then: "F5" to run.  
After a while (program is transferred serially to the Atari), a "Hello world!" should appear on your Atari display.  

## How this works
Remote connections in GDB can be done in two ways: "remote" and "extended-remote", where the latter is used in this example.  
Extended-remote enables us to make a connection to gdbserver without automatically executing the program we want to debug. This makes it possible to use remote file operations to transfer the executable to the Atari and then execute it.  

The way this is done from VsCode is defined in the ".vscode/launch.json" file.  
The line:  
´´´
"target": {"type": "extended-remote" ,"parameters": ["${env:GDB_COM_PORT}"]}
´´´
Defines that we want "extended-remote" and that the connection should be whatever is in the "GDB_COM_PORT" environment variable that is set in the "vscode.sh" script.  
  
The settings that transmits the file and starts debugging is a bit of a hack...  
It feels like the CDT GDB extension is missing some functions, but the following lines makes it work:
´´´
"preRunCommands": [
	"print \"just a nop\""
],
"initCommands": [
	"remote put ${workspaceFolder}/build/${env:TARGET_NAME}.prg ${env:TARGET_NAME}.prg",
	"set remote exec-file ${env:TARGET_NAME}.prg",
	"starti"
],
´´´
"initCommands" are GDB commands that are run after a connection have been established.  
Those commands will transmit the executable to the Atari, set the executable name to be debugged, and finally start execution but stop at the very first instruction.  
"preRunCommands" are GDB commands that are run before the debugged executable starts.  
But wait, the executable is already started in "initCommands"?  
And that is the hack:  
If we removed the "starti" and "preRunCommands", then the CDT GDB extension would just disconnect after having transmitted the file. It would not execute the file.  
And if you put a "run" instead of "starti", then the debugger would run, but breakpoints set in the editor would not be set in the gdbserver...  
So by starting with "starti" which breaks at first instruction, and then doing a "print \"just a nop\"", will trick the CDT GDB extension into a state where the executable is started and then paused.  
This allows the breakpoints to be set and execution to continue.  
Hopefully, this hack can be removed in the future.

## Faster debug
If you have started and stopped debugging of an executable and want to do it again without transfering the executable, then just comment out the following line in "launch.json":
´´´
	"remote put ${workspaceFolder}/build/${env:TARGET_NAME}.prg ${env:TARGET_NAME}.prg",
´´´
