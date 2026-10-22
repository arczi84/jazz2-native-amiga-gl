# GCC 16 AmigaOS/m68k: `-mhard-float` selects a soft-float multilib

## Summary

On the tested AmigaOS/m68k GCC 16 installations, compiling for a 68060 with
`-mhard-float` enables the hard-float calling convention in generated code, but
the GCC driver still selects the `libm060` multilib built for soft-float. This
mixes hard-float callers with soft-float runtime helpers from `libgcc.a`.

In particular, hard-float callers expect scalar `float` results in `FP0`, while
the selected `libm060` implementation of `__floatundisf` ultimately returns the
result in `D0`. This produces incorrect values for conversions such as
`uint64_t -> float`.

This is not a libnix issue. The mismatch is between compiler-generated
hard-float code and the selected GCC multilib/runtime.

## Tested compilers

The issue is present in both installations below:

```text
/opt/amiga-gcc16/bin/m68k-amigaos-gcc
m68k-amigaos-gcc (GCC) 16.2.0b 20260825082934

/opt/amiga-gcc16-bernie/bin/m68k-amigaos-gcc
m68k-amigaos-gcc (GCC) 16.2.0b 20260825082934
```

The Bernie variant generates somewhat different object code, but has the same
multilib selection and the same scalar float return ABI mismatch.

## Reproduction: option state and multilib selection

Run the following commands for either compiler installation:

```sh
m68k-amigaos-gcc -mcpu=68060 -Q --help=target \
    | grep -E 'm68881|mhard-float|msoft-float'
m68k-amigaos-gcc -mcpu=68060 -print-multi-directory

m68k-amigaos-gcc -mcpu=68060 -mhard-float -Q --help=target \
    | grep -E 'm68881|mhard-float|msoft-float'
m68k-amigaos-gcc -mcpu=68060 -mhard-float -print-multi-directory
m68k-amigaos-gcc -mcpu=68060 -mhard-float -print-file-name=libgcc.a

m68k-amigaos-gcc -mcpu=68060 -m68881 -print-multi-directory
m68k-amigaos-gcc -mcpu=68060 -m68881 -print-file-name=libgcc.a
```

Observed result:

```text
-mcpu=68060
  -m68881      [disabled]
  -mhard-float [disabled]
  -msoft-float [enabled]
  multilib: libm060

-mcpu=68060 -mhard-float
  -m68881      [enabled]
  -mhard-float [enabled]
  -msoft-float [disabled]
  multilib: libm060
  libgcc: .../libm060/libgcc.a

-mcpu=68060 -m68881
  -m68881      [enabled]
  -mhard-float [enabled]
  -msoft-float [disabled]
  multilib: .
  libgcc: .../libgcc.a
```

Therefore, `-mhard-float` changes code generation and the ABI, but does not
change the selected multilib. Both plain `-mcpu=68060` (soft-float) and
`-mcpu=68060 -mhard-float` select the same soft-float `libm060` directory.

## Minimal caller ABI demonstration

Given:

```c
extern float convert(unsigned long long value);

float use(unsigned long long value)
{
    return convert(value) + 1.0f;
}
```

The tested GCC generates the following relevant code in soft-float mode:

```asm
jsr     _convert
move.l  d0,-(sp)       ; scalar float result is read from D0
jsr     ___addsf3
```

With `-mhard-float`, it generates:

```asm
jsr     _convert
fsadd.s #0x3f800000,fp0 ; scalar float result is read from FP0
```

The caller ABI therefore definitely changes between the two modes.

## Runtime helper mismatch

The selected archive is:

```text
.../lib/gcc/m68k-amigaos/16.2.0b/libm060/libgcc.a
```

Its `_floatundisf.o` implements `__floatundisf` through soft runtime helpers and
ends by calling `__truncdfsf2`:

```asm
jsr     ___truncdfsf2
addq.l  #8,sp
movem.l (sp)+,...
rts
```

The `libm060` `__truncdfsf2` implementation returns the single-precision bit
pattern in `D0` and does not transfer it to `FP0` before `rts`.

In a real hard-float application binary, a typical call site is:

```asm
jsr     __floatundisf
fmovex  %fp0,sp@(20)    ; caller immediately consumes FP0
```

Thus the hard-float caller consumes an undefined/stale `FP0` value even though
the correct result is present in `D0`.

## Relevant GCC configuration

The generated specs contain:

```text
*multilib_options:
fbaserel/fbaserel32 mcpu=68020/mcpu=68060 m68881
```

The corresponding AmigaOS target configuration is:

```make
MULTILIB_OPTIONS    = fbaserel/fbaserel32 mcpu=68020/mcpu=68060 m68881
MULTILIB_DIRNAMES   = libb libb32 libm020 libm060 libm881
MULTILIB_EXCEPTIONS = fbaserel/m68881 fbaserel/mcpu=68060/m68881 \
                      fbaserel32 fbaserel32/m68881 \
                      fbaserel32/mcpu=68060/m68881 \
                      m68881 mcpu=68060/m68881
```

This configuration uses the command-line spelling `m68881` as a multilib
dimension, but does not account for `mhard-float`.

At the same time, `gcc/config/m68k/m68k.opt` defines both options using the same
backend mask:

```text
m68881
Target RejectNegative Mask(HARD_FLOAT)

mhard-float
Target RejectNegative Mask(HARD_FLOAT)
```

Consequently, the backend treats `-mhard-float` as hard-float, while multilib
selection does not treat its spelling as equivalent to `-m68881`.

`LINK_CPU_SPEC` already recognizes both spellings:

```c
%{mhard-float|m68881|...:-fl libm881}
```

The inconsistency is specifically in GCC multilib selection.

## User-visible impact

The issue was found in a game where the frame timer converts a 64-bit clock
counter to `float`. The compiler inserts `__floatundisf` automatically for that
conversion. A corrupted frame duration then produces incorrect physics,
collision and cooldown state.

A completely soft-float build works correctly because both callers and runtime
helpers use `D0`. The mixed build can still be significantly faster because
most floating-point arithmetic is emitted as FPU instructions; only calls that
cross the mismatched runtime ABI return incorrect values. It is therefore
possible for the affected hard-float build to be faster while remaining
incorrect.

## Expected behavior

`-mhard-float` and `-m68881` both enable `HARD_FLOAT`, so they must select ABI-
compatible multilibs. In particular, every scalar floating-point helper linked
into a hard-float application must return its result according to the hard-
float ABI (`FP0` for `float` in the demonstrated case).

## Validated fix

The fix was tested against the public `AmigaPorts/m68k-amigaos-gcc` wrapper
with GCC branch `amiga16.2`, GCC commit `9529cbe26`. Add this line to
`gcc/config/m68k/t-amigaos`:

```make
MULTILIB_MATCHES    = m68881=mhard-float
```

This makes the driver treat the two command-line spellings as equivalent for
multilib selection. It does not add another library variant: the existing
hard-float `libm020/libm881` runtime is selected through the target's existing
`-fl libm020 -fl libm881` linker mechanism.

The libnix startup selection also needs to recognize the alias. In
`gcc/config/m68k/m68kamigaos.h`, change:

```c
"%{!m68881:-u___cpucheck }"
"%{m68881:-u___fpucheck }"
```

to:

```c
"%{!mhard-float:%{!m68881:-u___cpucheck }}"
"%{mhard-float|m68881:-u___fpucheck }"
```

GCC, libgcc and libstdc++ were rebuilt after applying these changes. The fixed
driver now produces:

```text
$ m68k-amigaos-gcc -mcpu=68060 -mhard-float -print-multi-directory
.
$ m68k-amigaos-gcc -mcpu=68060 -m68881 -print-multi-directory
.
```

For `-mcpu=68060 -mhard-float -mcrt=nix20`, the linker command contains both
`-fl libm020 -fl libm881` and `-u___fpucheck`.

A linked regression program performing `uint64_t -> float` was disassembled.
Its caller consumes `FP0`, and the linked `__floatundisf` ends every return path
with the required ABI bridge:

```asm
fmoves  %fp0,d0
fmoves  d0,%fp0
rts
```

Finally, the complete game was rebuilt in Release mode with
`-mcpu=68060 -mhard-float -O2 -ffast-math`. The final Amiga Hunk executable
contains `__fpucheck`; its timer call sites consume `FP0` immediately after
`__floatundisf`, and the helper embedded in that same executable contains the
two `fmoves` instructions above. This validates the fix in the original
application, not only in a standalone object test.

An audit of other `libgcc` helpers returning scalar `float` or `double` would
still be a useful additional regression test, but the multilib alias fixes the
general cause by selecting an ABI-compatible runtime rather than special-casing
`__floatundisf`.
