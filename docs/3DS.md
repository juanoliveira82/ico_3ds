# ICO on the Nintendo 3DS

This document covers the Nintendo 3DS port: its goals, how to build it, how to
run it and where it stands. The port lives on the `3ds` branch of this
repository. The `main` branch mirrors [ico-pc](https://github.com/nathanialf/ico-pc)
without changes, so upstream fixes can be merged in.

## Goals

- **The Old 3DS first.** The port targets the original Nintendo 3DS (and 2DS):
  a 268 MHz ARM11, 64 MB of application memory in the normal memory mode, and
  the PICA200 GPU. Any New 3DS runs what an Old 3DS runs.
- **The game's own code.** Like ico-pc, the port runs the reconstructed source
  of the PS2 game (`ico2/`). The 3DS adds a platform layer (`port/3ds/`) and,
  later, a renderer for the PICA200. The game's logic does not change.
- **Small changes to the shared code.** Everything 3DS-specific stays in
  `port/3ds/` and `cmake/Ico3ds.cmake`. Shared files get a 3DS branch only
  where the 3DS has no equivalent of what they call (no `mmap`, no signals,
  no `statfs`), behind `ICO_3DS`. This keeps merges from upstream easy.

## Milestones

| # | Milestone | State |
|---|-----------|-------|
| 1 | **Headless build**: the game boots and runs its threads on the 3DS, nothing is drawn. The top screen shows the speed and the memory figures; the bottom screen shows the log. | Code written, not yet run on hardware |
| 2 | Disc image read from the SD card, game data extracted, the title screen reached (in the log) | Not started |
| 3 | First picture: the opaque world (list 0) and the UI (list 11) drawn with citro3d | Not started |
| 4 | Transparency, shadows, particles, sound, the FMVs | Not started |
| 5 | Speed and memory work for the Old 3DS: a smaller arena, simplified effects | Not started |

Milestone 1 is mostly about measurement. The figures it shows decide how
milestone 5 has to cut memory.

## Memory: the main problem

The PC port keeps the PS2's 32 MB of main RAM as one block (the "arena",
`port/platform/arena.h`) plus 8 MB of headroom, 40 MB in all. Of that
headroom, 7 MB is there because the game's records grow on a 64-bit host.
On the 3DS pointers are 32-bit, as on the PS2, so most of it can probably
be removed. That still has to be measured.

For scale, the headless PC build (x86-64) has about 7.3 MB of code, 0.7 MB of
initialised data and 16 MB of zeroed data (`.bss`). The ARM build will differ
somewhat. Code plus static data plus the arena already comes close to the
64 MB an Old 3DS application gets, which is why the first milestone measures
first.

How the 3DS build divides memory (`port/3ds/heap_3ds.c`):

| Share | Size | Used for |
|-------|------|----------|
| Program image | fixed at link time | code, data, `.bss` |
| newlib heap | 12 MB (`ICO_3DS_NEWLIB_HEAP_BYTES`) | the port's `malloc`: the disc reader, the archive index, decoders |
| linear heap | 4 MB (`ICO_3DS_LINEAR_HEAP_BYTES`) | GPU and DSP buffers (small while nothing is drawn) |
| reservations | the rest | the EE arena and the game threads' stacks, committed page by page (`port/3ds/vm_reserve_3ds.c`) |

If the figures do not fit, the options are:

1. A smaller arena: measure the game heap's high-water mark (the log's
   `heap: ... high water` lines) and cut the headroom.
2. A bigger memory mode: an Old 3DS application installed as a CIA can ask
   for 72, 80 or 96 MB, at the cost of some system features (the HOME Menu
   cannot run beside it). This needs a CIA build and testing.
3. Smaller static buffers in the port layer (the largest `.bss` objects are
   listed by `nm --size-sort` on the ELF).

## Building

You need [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the 3DS
packages, CMake 3.25 or newer, Ninja and Python 3.

1. Install devkitPro's 3DS tools (on Windows, use the installer and pick the
   3DS development option; on Linux and macOS, use `dkp-pacman`):

   ```sh
   sudo dkp-pacman -S 3ds-dev
   ```

   Then check that `DEVKITPRO` is set (usually `/opt/devkitpro`):

   ```sh
   echo $DEVKITPRO
   ```

2. Fetch the two libraries the build compiles from source: libchdr (the
   `.chd` disc image reader) and libmpeg2 (the FMV decoder). The fetch script
   can skip everything the 3DS build does not use:

   ```sh
   SKIP_SDL3_LINUX=1 SKIP_SDL3_MINGW=1 SKIP_APPIMAGE=1 SKIP_VALIDATION_LAYER=1 \
   SKIP_DXC=1 SKIP_SPIRV_CROSS=1 SKIP_FFX=1 SKIP_NOTO_JP=1 SKIP_NOTO_KR=1 \
   SKIP_NOTO_TC=1 SKIP_ARIMO=1 tools/fetch_deps.sh
   ```

3. Configure and build with the `3ds` preset:

   ```sh
   cmake --preset 3ds
   cmake --build --preset 3ds
   ```

   The result is `build-host/3ds/ico.3dsx`.

## Running

1. Copy `ico.3dsx` to `sdmc:/3ds/` on the SD card.
2. Create the folder `sdmc:/3ds/ico/` and copy your own disc image there,
   named after its disc: `Ico_PAL.iso` for the European disc (SCES-50760),
   `Ico_USA.iso` for the North American one (SCUS-97113), `Ico_JPN.iso` for
   the Japanese one (SCPS-11003); `.chd`, `.bin` and `.cue` work too. The SD
   card is usually FAT32, which cannot hold a file of 4 GB or more: use a
   `.chd` image if the `.iso` is that big.
3. Start **ICO** from the Homebrew Launcher.

The first start extracts the game data from the image into `sdmc:/3ds/ico/`
(about 1 GB). This takes a while on the 3DS.

On screen:

- **Top screen:** the status panel, with the simulated vsyncs, the game's
  main tick and the memory figures.
- **Bottom screen:** the log. The same log is written to
  `sdmc:/3ds/ico/ico-3ds.log`.
- **START + SELECT** quits. After the run ends the screens stay up until
  START is pressed, so an error can be read.

In the headless milestone the simulation runs as fast as it can rather than
at 60 vsyncs a second, so the speed figure shows how much headroom (or how
little) the Old 3DS has.

## What the 3DS layer replaces

| PC | 3DS (`port/3ds/`) | Why |
|----|-------------------|-----|
| `main()` in `main_host.c` | `main_3ds.c` calls `ico_host_main` | the screens, the SD folder and the log come first |
| `vm_reserve.c` (`mmap`, `VirtualAlloc`) | `vm_reserve_3ds.c` (`svcControlMemory`) | no `mmap`; pages are committed in the heap area |
| libctru's default heaps | `heap_3ds.c` | the reservations need part of the memory budget |
| the log file and the console | `log_3ds.c` | one stream to the bottom screen and the SD card |
| the window | `status_3ds.c` | the status panel, the HOME button and quitting |

The shared files changed for the 3DS, each behind `ICO_3DS` or a 32-bit
check:

- `port/platform/fpenv.c`: the EE's float mode (round toward zero, flush to
  zero) on 32-bit Arm through FPSCR.
- `port/math/softdouble.c`: the EE's double arithmetic without 128-bit
  integers (checked bit for bit against the 128-bit version on 20 million
  random operands).
- `port/platform/fiber.h`, `fiber.c`, `arena.h`: smaller stacks and no pinned
  addresses, set by `cmake/Ico3ds.cmake`.
- `port/platform/diag_host.c`, `state_check.c`, `host_fs.c`, `report.c`,
  `ico_thread.c`, `host_config.c`, `main_host.c`: no signal handlers, no
  program header walk, no `statfs`, no `tm_gmtoff`, no `sysconf`; the SD card
  folder as the program's folder; the 3DS entry point.
- `ico2/fumi/src/boyact.c`: a layout check that assumed a 64-bit host now
  accepts the EE's own 32-bit layout.

## How the code was checked so far

- Every source of the headless program (507 files) was compiled for 32-bit
  Arm (`armv6k`, hard float) against newlib's and libctru's headers. All of
  them compile, apart from `vm_reserve.c`, which the 3DS build replaces.
- The headless PC build still builds and its tests pass.
- Nothing has run on a 3DS yet. Expect link errors and run-time problems in
  the first real build. Please send the log (`ico-3ds.log`) and the
  build output.
