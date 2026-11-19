# GDB server for Atari TOS computers.

An Atari specific gdb server supporting motorola 68000 and up to 68060 CPUs.

## Build

    make TOOLKIT=$HOME/toolchain/m68k-atari-elf all

- Making the project will build a `gdbsrv.ttp` file that can be run in Hatari.

## Test

- Use the example found at: "../extras/hello_world" to build a test program to debug. Observe that it will produce two important files: `hello` that is an elf executable containing all debug symbols, and `hello.prg` is a clean atari executable.
- In Hatari, start `gdbsrv.ttp` with the arguments: "--multi hello.prg" where `hello.prg` is the program to debug.
- Use `gdb-multiarch` to connect to Hatari simulated RS232 ports to send and recieve packets from `gdbsrv.ttp`.
- If needed, use socat to create virtual com ports between Hatari and gdb-multiarch: `socat PTY,link=$HOME/st_a,raw,echo=0 PTY,link=$HOME/st_b,raw,echo=0` Where `$HOME/st_a` is the port gdb-multiarch connects to, and `$HOME/st_b` is used as Hatari command line arguments as: `--rs232-in $HOME/st_b --rs232-out $HOME/st_b`
