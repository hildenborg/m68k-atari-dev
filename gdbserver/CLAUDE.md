# GDB server for Atari TOS computers.

An Atari specific gdb server supporting motorola 68000 and up to 68060 CPUs.

## Build

    make TOOLKIT=$HOME/toolchain/m68k-atari-elf all

- Making the project will build a `gdbsrv.ttp` file that can be run in Hatari.
- The makefile builds into the current directory, so build from a separate directory to keep object files out of the sources: `mkdir build && cd build && make -f ../makefile TOOLKIT=$HOME/toolchain/m68k-atari-elf all` (as `build.sh` does).
- There is no C library and no libgcc. Code that makes gcc emit library calls fails to link:
	- Struct assignment can become `memcpy`. Copy fields one by one instead.
	- 32x32 bit multiplication on 68000 becomes `__mulsi3`. Keep multiplications 16x16 bits (`mulu.w`).
	- Check with `m68k-atari-elf-nm gdbsrv | grep " U "` that nothing is undefined.
- `critical.s` is assembled for 68030/68882. It is the only place for 68020+ and FPU instructions, and they must be guarded by checks of `Cookie_CPU` / `Cookie_FPU`.
- GEMDOS/BIOS/XBIOS calls are wrapped in `bios_calls.c`. Calls where the OS writes to memory through a pointer argument need a `"memory"` clobber.

## Test

- Use the example found at: "../extras/hello_world" to build a test program to debug. Observe that it will produce two important files: `hello` that is an elf executable containing all debug symbols, and `hello.prg` is a clean atari executable.
- In Hatari, start `gdbsrv.ttp` with the arguments: "--multi hello.prg" where `hello.prg` is the program to debug.
- Use `gdb-multiarch` to connect to Hatari simulated RS232 ports to send and recieve packets from `gdbsrv.ttp`.
- If needed, use socat to create virtual com ports between Hatari and gdb-multiarch: `socat PTY,link=$HOME/st_a,raw,echo=0 PTY,link=$HOME/st_b,raw,echo=0` Where `$HOME/st_a` is the port gdb-multiarch connects to, and `$HOME/st_b` is used as Hatari command line arguments as: `--rs232-in $HOME/st_b --rs232-out $HOME/st_b`
- When testing on Falcon, connect the socat virtual com port to Hatari using: `--scc-b-in $HOME/st_b --scc-b-out $HOME/st_b`

### Running Hatari unattended

- `hatari-prg-args` autostarts a program with arguments, so no desktop interaction is needed. Run headless with a host folder as drive C:

		SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy hatari-prg-args \
			--machine ste --tos $HOME/tos/tos206.img --fast-boot true --sound off --confirm-quit false \
			--harddrive <dir> --gemdos-drive C --rs232-in $HOME/st_b --rs232-out $HOME/st_b \
			-- <dir>/GDBSRV.TTP --multi HELLO.PRG

- TOS images are in `$HOME/tos`: `tos206.img` for `--machine st/ste`, `etos512se.img` (EmuTOS) for `--machine tt/falcon`, `tos404.img` for Falcon.
- The TT (68030 + 68882) exercises the FPU code paths. Hatari's default FPU emulation never sets the inexact flag or takes FPU exception traps, so add `--fpu 68882 --fpu-softfloat true` when testing FPU exceptions.
- Drive C: is a host folder, so files written through `vFile` packets can be checked directly on the host.
- GEMDOS file times are local time without time zone, so `stat` times are the host time plus the time zone offset.
- Stop with `pkill -x hatari` and `pkill -x socat`. A `pkill -f` pattern can match the command line of the shell running it and kill that shell.

### gdb session

	set architecture m68k
	set serial baud 9600
	file hello
	target extended-remote $HOME/st_a
	break main
	continue

- Load the elf (`file hello`) before connecting. Without it gdb assumes little endian and shows all registers byte swapped; use `set endian big` if connecting without a file.
- To run another program in the same server: `set remote exec-file C:\\OTHER.PRG` then `run`.
- `-batch` with `-x script.gdb` works well for scripted tests. When gdb exits it kills the inferior with `vKill`, and the server keeps running with `--multi`.

### Serial line limits

- Hatari delivers bytes from the pty faster than a real 9600 baud line and ignores RTS/CTS. The server therefore replaces the 256 byte TOS serial input buffer with a 2 KB one (`comm.c`), so a whole gdb packet (max 1023 bytes, more when escaped) fits.
- A custom test client talking to the pty directly should keep each burst below 2 KB.
