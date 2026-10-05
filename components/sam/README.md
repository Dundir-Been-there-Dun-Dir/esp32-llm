# SAM — Software Automatic Mouth

Text-to-speech used by `main/speak.c`. Copied from
[s-macke/SAM](https://github.com/s-macke/SAM) at commit `a7b36ef` (`src/`, without
its SDL/CLI `main.c`).

**License:** none. SAM is a reverse-engineered version of the 1982 Commodore 64
program by Don't Ask Software; upstream's README states it has no open-source
license and points to fair use.

Changes for the ESP32:

- `sam.c` `Init()`: allocate the 220 KB output buffer once instead of on every
  `SAMMain()` call (upstream leaks it each time).
- `CMakeLists.txt`: `-fcommon`, because `sam.c` and `reciter.c` both define the
  globals `A`, `X`, `Y` (GCC 10+ defaults to `-fno-common`); `-w` for the 1982-era
  warnings. The `debug` global SAM expects is defined in `main/speak.c`.
