# GDB server for Atari TOS computers.

An Atari specific gdb server supporting motorola 68000 and up to 68060 CPUs.

## Build

    make TOOLKIT=$HOME/toolchain/m68k-atari-elf all

- Making the project will build a `gdbsrv.ttp` file that can be run in Hatari.
- The makefile builds into the current directory, so build from a separate directory to keep object files out of the sources: `mkdir build && cd build && make -f ../makefile TOOLKIT=$HOME/toolchain/m68k-atari-elf all` (as `build.sh` does). `build.sh` also copies the result to `$HOME/sthd/gdbsrv.ttp`.
- There is no C library and no libgcc. Code that makes gcc emit library calls fails to link:
	- Struct assignment can become `memcpy`. Copy fields one by one instead.
	- 32x32 bit multiplication on 68000 becomes `__mulsi3`. Keep multiplications 16x16 bits (`mulu.w`).
	- Check with `m68k-atari-elf-nm gdbsrv | grep " U "` that nothing is undefined.
- `critical.s` is assembled for 68030/68882. It is the only place for 68020+ and FPU instructions, and they must be guarded by checks of `Cookie_CPU` / `Cookie_FPU` (or `Fpu_Coprocessor`, which is 0 when there are no FPU registers to save). Comments in `.s` files use `|`; `//` does not assemble.
- GEMDOS/BIOS/XBIOS calls are wrapped in `bios_calls.c`. Calls where the OS writes to memory through a pointer argument need a `"memory"` clobber.
- `link.ld` must collect wildcard input sections (`.text*`, `.data*`, `.bss .bss.*`), or `-fdata-sections` style sections land outside the segment sizes written to the prg header.

## Runtime model

- The server runs the inferior with `Pexec`. While no inferior is stopped, the server runs in user mode (`isSupervisorMode` false in the command loop): privileged access, like memory reads and writes, must go through `Supexec`.
- While an inferior is stopped, the server runs inside the exception handler in supervisor mode. Memory accesses then go through `InferiorContextMemoryAddress`, which returns the inferior's saved vectors, MFP and video registers instead of the server's live ones.
- Serial communication goes through the TOS BIOS `AUX` device for both the MFP (ST/STE/TT) and the SCC (Falcon, mapped with `Bconmap(7)`). Only the receive interrupt is hooked (`comm_mfp_scc.s`), to catch CTRL-C from gdb while the inferior runs. The SCC hook must cover both channel B receive vectors, 0x190 and 0x198: Hatari delivers received bytes through 0x198.
- GEMDOS is not reentrant. When the inferior stopped in supervisor mode outside its own text (`osMaybeBusy`), it may be inside a GEMDOS call, so the server must not call GEMDOS then (vFile, working dir, log file). BIOS and XBIOS calls are used instead where possible.

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
- For 68040/68060 use the TT with `--cpulevel 4` or `--cpulevel 6` and `--data-cache true`. The Falcon's serial port needs `--scc-b-in/--scc-b-out`, not `--rs232-*`.
- Hatari does not emulate an SFP004 FPU. Test that path with a build that forces the `_FPU` cookie.
- The TT (68030 + 68882) exercises the FPU code paths. Hatari's default FPU emulation never sets the inexact flag or takes FPU exception traps, so add `--fpu 68882 --fpu-softfloat true` when testing FPU exceptions.
- Drive C: is a host folder, so files written through `vFile` packets can be checked directly on the host.
- GEMDOS file times are local time without time zone, so `stat` times are the host time plus the time zone offset.
- `--conout 2` copies the console output (e.g. `ConOut` messages) to Hatari's stdout.
- A blank floppy for drive A: (e.g. to test drive changes): `hmsa blank.st DS`, then `--disk-a blank.st`.
- Run Python RSP clients with `python3 -u`, or their output is lost when `timeout` kills them.
- `hatari-prg-args` passes the arguments with a leading space in the command line.
- Addresses that give a bus error on ST/STE: `0x00f10000` and `0xffff8e00`. `0xffff8010` is a readable/bus error boundary.
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
- `QStartNoAckMode` is advertised, so gdb uses no-ack mode. It lasts for one connection: the server ends it on `D`, on a detected disconnect, and when a packet starting with `qSupported` arrives (gdb starts every connection with it). A serial line usually can't signal a disconnect, so a custom test client that uses no-ack mode should end with `D`, or the next client should start with `qSupported`. `vKill` doesn't end it, as gdb stays connected after a kill.
- Extended mode (`!`) is reset by `qSupported` only, not by `D` or a disconnect: after a kill it decides whether a server started without `--multi` keeps running for the next connection. A test client that relies on extended mode being off should start with `qSupported`.
- In no-ack mode gdb sends its next packet right after a reply. While the inferior runs, the CTRL-C hook drops every other received byte, so it must be off before the server replies (e.g. `W` after the inferior exits).
