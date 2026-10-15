# Make it yours

LCD-4 is free software (GPL-3.0). You're welcome to fork it, change it, rename it and release your own
version; no need to ask. You can also send changes back here. Both are described below.

## What the licence asks

If you give someone a firmware built from this code (a release, a `.fwsc` in a chat, a page with an
installer), give them its complete source under the same licence, and keep the copyright notices and
`LICENSE` that are in the files. That's all. The release workflow and the site generator already do it:
every release's tag is its source, and the site carries a tarball of it.

## Build it, try it, test it

Everything runs on GitHub: a fork builds and tests every push (**Actions → Build and test**), and the firmware
is attached to the run as an artifact to download. On your own machine:

| | |
|---|---|
| `tests/run_tests.sh` | the tests (the instrument, the platform, the app in a simulator, the browser build if Emscripten is there) |
| `./build.sh` | the firmware, `build/lcd4.fwsc` (needs the JieLi toolchain and three SDK files; see [BUILDING.md](BUILDING.md)) |
| `build/host/lcd4_host SCRIPT OUT` | the whole app on your computer from a script, with the screen as PNGs and the sound as WAV (`tests/scenarios/*.lcd`) |
| `web/emu/build.sh` | the browser emulator (needs Emscripten) |
| `python3 tools/fm1_install.py build/lcd4.fwsc` | install it over USB |
| `python3 tools/fm1_status.py load` | the audio load on the FM-1 itself, and where the time goes |

On a Mac the build runs the JieLi tools in Docker; on Linux x86-64 (and on GitHub) it runs them directly.
`tools/get_toolchain.sh` and `tools/get_sdk_files.sh` fetch what it needs.

## Where things are

| | |
|---|---|
| `firmware/src/dsp/lcd4.c` | the sequencer, the mix, the commands, the render (one unit, with the files below) |
| `firmware/src/dsp/lcd4_drums.c` | the four kits |
| `firmware/src/dsp/lcd4_synth.c` | the sixteen sounds, their voices, the arp, the chorus |
| `firmware/src/dsp/lcd4_fx.c` | the delay, the plate, the punch-in effects, the compressor |
| `firmware/src/dsp/lcd4_demo.c` | the demo songs (in a small text notation) |
| `firmware/src/app/ui.c` | the controls, the pages, the screen |
| `firmware/src/app/scenes.c` | the LCD scenes and their art |
| `firmware/src/app/project.c` | saving and loading |
| `firmware/hal/`, `firmware/loader/`, `firmware/src/{usb,storage,ota}.c` | the FM-1 platform (Felucca's) |
| `host/`, `tests/` | the simulator and the tests |
| `web/`, `tools/make_pages.py` | the emulator, the installer, the site |

## Your own firmware, from this one

To release it under your own name, so that it and LCD-4 can live side by side (and on the
[FM-1 firmware hub](https://thefirmwarehub.com/)):

1. **The name.** Search for `LCD-4` and change what you want: the start screen (`firmware/src/app/main_fm1.c`),
   the USB names (`STR1` and `STR2` in `firmware/src/usb.c`), the docs and the site.
2. **The identity**, what the FM-1 reports and the installers check: `PRODUCT` and the release prefix in
   `tools/build.py`, and the `FELUCCA_ID` default in `firmware/src/main.c`. Pick a prefix nobody uses (LCD-4
   is `FM-1_4…`); the hub's [catalogue](https://thefirmwarehub.com/catalog.json) lists the ones in use.
3. **The saved data's tags**, `PROJ_MAGIC` and `BANK_MAGIC` in `firmware/src/app/app.h`, so that your
   firmware and LCD-4 never read each other's saves.
4. **Releases:** push a tag `vX.Y.Z`; the Release workflow builds and publishes it. For the site, set
   **Settings → Pages → Source** to **GitHub Actions**; the Pages workflow then publishes it after each
   release.

## Sending changes back

Pull requests are welcome, small or large; there's no form to fill in. It helps if:

- `tests/run_tests.sh` passes (the pull request's checks run it for you);
- new behaviour has a test or a line in a scenario script, and the manual (`docs/MANUAL.md`) says how to use it;
- you say whether you tried it on an FM-1, and what `tools/fm1_status.py load` showed for anything that
  makes sound.

Some habits of the code: C99, `float` only (the FM-1 has no double-precision hardware, and the build fails if
a `double` gets in), no libm (`fastmath.h` instead), no divisions in per-sample loops, and loops that keep
few values alive (the FM-1's floats share its sixteen registers). The comments say what a thing is for,
in plain words.
