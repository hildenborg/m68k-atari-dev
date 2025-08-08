# m68k-atari-dev
A development system providing cross compiling and remote debugging for Atari TOS computers.

## Key features:
* Standard C and C++ libraries using newlib [m68k-atari-elf](https://sourceware.org/git/?p=newlib-cygwin.git;a=commit;h=cac47030fb003570295582606f158609f626347f) target.
* Elf to prg converter that keeps binary segments unchanged for symbol compatibility.
* A gdbserver specifically written from the ground up for Atari TOS computers.
* Editing, building and debugging can all be integrated in Visual Studio Code.

## Setup for Linux (Recommended! Tested on Ubuntu):
1. Open a terminal.
2. Enter: `sudo apt update`
3. Enter: `sudo apt install build-essential texinfo flex bison libgmp-dev libmpfr-dev libmpc-dev gdb-multiarch`
4. Enter: `git clone https://github.com/hildenborg/m68k-atari-dev.git` to download m68k-atari-dev.
5. Enter: `cd m68k-atari-elf`
6. Enter: `./build.sh`
   
Depending on your system, the last line may take minutes or hours.  
All binaries will be installed in (UserHome)/toolchain/m68k-atari-elf.  

## Setup for Macos (debugging currently not supported):
1. Open a terminal.
2. Enter: `xcode-select --install` to install all xcode command line tools.
3. Enter: `git clone https://github.com/hildenborg/m68k-atari-dev.git` to download m68k-atari-dev.
4. Enter: `cd m68k-atari-elf`
5. Enter: `./build.sh`
   
Depending on your system, the last line may take minutes or hours.  
All binaries will be installed in (UserHome)/toolchain/m68k-atari-elf.  

## Setup for Windows (not recently tested, expect problems):
1. Install msys and mingw by following instructions on [www.msys2.org](https://www.msys2.org/)
2. From start menu, start `MSYS2 UCRT64`.
3. Enter: `pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain cpio gmp-devel mpfr-devel mpc-devel git mingw-w64-ucrt-x86_64-gdb-multiarch`
4. Enter: `git clone https://github.com/hildenborg/m68k-atari-dev.git` to download m68k-atari-dev.
5. Enter: `cd m68k-atari-elf`
6. Enter: `./build.sh`
   
Depending on your system, the last line may take minutes or hours.  
All binaries will be installed in (UserHome)/toolchain/m68k-atari-elf.  

## Examples:
There are a few examples in the [Extras](extras/README.md) folder.

## TOS specific C/C++ features:
The stack is defaulted to 2000 bytes deep. This can be changed by adding the following line anywhere in your code:
`unsigned int _STACK_SIZE = [wanted stack size];`
The heap is defaulted to use the rest of the available memory. This can be changed by adding the following line anywhere in your code:
`unsigned int _HEAP_SIZE = [wanted heap size];`

## Elf to prg technical:
Conversion from elf to prg is done in two separate steps: link time and post link time.  
Link time is done through a link script included in newlib, installed in "toolchain/m68k-atari-elf/m68k-atari-elf/lib/atari-tos.ld".  
Post link time is done by the binary "m68k-atari-elf-prg" which is built from the sources in "elf-prg".  
The link script will:  
Collect all relevant sections and bunch them together in the three segments atari prg files support: ".text", ".data" and ".bss".  
The ".text" segment will start at address 0 and the ".data" and ".bss" segments will follow accordingly.  
A segment called ".prgheader" containing all correct data will be included in the elf file.  
The "m68k-atari-elf-prg" will:  
Extract ".prgheader", ".text", ".data" and ".bss" in that order and add a fixup table for relocation data.  
**The main benefit of doing it in two steps is that the elf file and prg file will be _binary address compatible_.**  
Which means that we can use the elf file for symbol lookup in gdb.  

## Gdbserver example usage:
Connect your atari to your pc using a serial to usb adapter.  
Copy the file "toolchain/m68k-atari-elf/bin/gdbsrv.ttp" and the program you want to debug to your Atari computer.  
On your atari, start "gdbsrv.ttp" and enter `--multi [name of prg to debug]`.  
In your pc terminal, enter: `gdb-multiarch [name of elf file]`.  
In gdb, enter: `target remote [pc serial port]`  
You can now remotely debug your atari code with full symbols.  
This can also be done using Visual Studio Code directly in source code, look at the "vscode_hatari" in "extras" for example of this.  
The "gdbsrv.ttp" program can be exited by pressing F4.  

## Contributing:
All contributions are welcome through pull request.  
Any fixes to the newlib/libgloss m68k-atari-elf target can be done through the [newlib mailing list.](https://sourceware.org/newlib/mailing.html)
