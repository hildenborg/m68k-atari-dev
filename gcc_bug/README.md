# GCC 16.1 m68k bug: wrong code from late-combine and post-increment

At `-O2`, GCC 16.1.0 can generate an instruction like this:

    move.w (%a0)+,(%a0,%d0.l)

The CPU increments `a0` before it calculates the destination address, so the
data is written 2 bytes too far. The code around the instruction expects the
address to be calculated with the old value of `a0`.

## Usage

    make          build build/gcc_bug.prg
    make check    compile copy.c with several options, and look for the bad instruction
    make run      run the program in Hatari
    make dumps    show the RTL before and after late_combine2

The toolchain is expected in `~/toolchain/m68k-atari-elf`. Use `TOOLKIT=<path>`
for another location, and `TOS=<image>` to choose the TOS for `make run`.

`make check` needs neither Hatari nor an Atari. `-O3` is reported as ok only
because it unrolls this short loop; it isn't a workaround.

## Test case

[copy.c](copy.c) is the function that is miscompiled:

    extern const unsigned short src[15];

    void copy(void)
    {
        int i;

        for (i = 0; i < 15; i++)
            ((volatile unsigned short *)0xffff8240UL)[i] = src[i];
    }

[main.c](main.c) runs it compiled with and without late-combine, and compares
the result in memory (the ST palette, which is restored afterwards).
Expected output:

    GCC 16.1.0
    -O2: WRONG
     got: 707 001 002 003 004 005 006 007
          010 020 030 040 050 060 070 100
     want: 001 002 003 004 005 006 007 010
          020 030 040 050 060 070 100 707
    -O2 -fno-late-combine-instructions: correct
    BUG REPRODUCED

(`707` is a marker, for an entry that hasn't been written.)

## Cause

Three passes are involved:

1. `ivopts` replaces the destination pointer with `src pointer + constant`.
2. `auto_inc_dec` turns the read into a post-increment. The result is still
   correct, because the destination address is calculated by a previous insn.
   After `combine` and register allocation:

       10: a1 = a0 + d0
       13: [a1] = [a0++]

3. `late_combine2` (after register allocation) substitutes insn 10 into insn 13:

       13: [a0 + d0] = [a0++]

After step 3, the two addresses in the insn share a register, and one of them
modifies it. The GCC internals manual (rtl.texi, node "Incdec") says:

> If a register used as the operand of these expressions is used in another
> address in an insn, the original value of the register is used.

So by that definition the RTL after step 3 still means the right thing. But the
m68k back end emits it as a single `move`, and the CPU doesn't work that way:
it uses the incremented value for the destination address. Whether
late-combine should refuse to create such an insn, or the m68k move patterns
should refuse to match it, is for the GCC developers to decide.

The options `-mlra` and `-m68000` to `-m68060` make no difference.

## Workarounds

- `-fno-late-combine-instructions`: disables the pass which creates the insn.
- `-fno-auto-inc-dec` or `-fno-ivopts`: also avoid it in this test case, but they
  only prevent the situation from arising, and affect more code.

`late_combine` is enabled at `-O2`, `-O3` and `-Os`. `-Os` happens to generate
correct code for this test case.

Only GCC 16.1.0 has been tested.
