## hello_world:
This is a minimal example, showing how to compile a "hello world" C program and convert it to an Atari executable.  

## lua:
This example builds the lua language for Atari.  
Run the `./download_and_build_lua.sh` script to do just that.  
The script will modify the lua code to properly build for Atari.  

## vscode_hatari:
This example will set up a hello world project with VsCode connected to Hatari.  
This currently do not work on windows and Macos.  
### Requirements:
It assumes that you have Visual Studio Code and Hatari installed.  
VsCode needs a native gdb debugger. In a terminal, enter: `code --install-extension webfreak.debug` to install.  
It also needs "socat" which on Ubuntu/Debian can be installed with `sudo apt install socat`.  
Hatari also needs a "sthd" folder in your home directory.  
"gdbsrv.ttp" should be copied to "sthd".  
### Use:
The script: "vscode.sh" will start Hatari and VsCode, and also use socat to create virtual serial ports connecting Hatari with VsCode.  
In Hatari: start "gdbsrv.ttp" and enter: `--multi hello.prg`  
In VsCode: press "Ctrl-LShift-B" to build program.  
In VsCode: press "F5" to debug program.  
After debugging, do not forget to click disconnect on the debugging menu, or you won't be able to start debugging again.  
Sometimes the connection becomes bad between Hatari and VsCode. A reset in Hatari usually solves it.  

## vscode_hatari_mintelf:
This example is the same as vscode_hatari but uses the mintelf toolchain.

