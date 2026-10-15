# Building LCD-4

The build makes three files in `build/`:

| File | What |
| --- | --- |
| `lcd4.bin` | the firmware app |
| `loader/ota.bin` | the update loader |
| `lcd4.fwsc` | the installable package (app + loader) |

## Prerequisites (macOS)

- Python 3 with Pillow: `pip3 install Pillow`
- Docker Desktop. The JieLi toolchain is Linux x86-64 only; the build runs each tool in a
  `linux/amd64` `debian:bookworm-slim` container (Rosetta on Apple silicon). Keep the source
  tree in a folder Docker can share, e.g. under `/Users`.
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server):

  ```
  tools/get_toolchain.sh            # installs to ~/.jieli/toolchain
  ```

- Three files of the JieLi AC79 SDK (Apache-2.0): `cpu/wl82/tools/uboot.boot`, `cfg_tool.bin` and
  `cfg/eq_cfg_hw.bin`. They are not part of this tree; this fetches just them, from JieLi's repository or a
  GitHub mirror of it, and keeps them only if their SHA-256 is the one the build expects:

  ```
  tools/get_sdk_files.sh            # into ~/fw-AC79_AIoT_SDK
  ```

  (or the whole SDK: `git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13
  https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK`)

- Node.js (optional, for the web tests).

On Linux x86-64 the toolchain runs natively and Docker is not needed.

## Build

```
./build.sh
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations
(`~/.jieli/toolchain`, `~/fw-AC79_AIoT_SDK`).

`./build.sh --release 0.1` makes a release build; the package is `build/lcd4-0.1.fwsc`.

`./build.sh --bench` makes a development build (`build/lcd4-bench.fwsc`) that also takes heavy test songs
over SysEx, to measure the audio load on the FM-1 itself. Every build answers the load query: send
`F0 7D 4C 34 01 F7` and the FM-1 replies `F0 7D 4C 34 01` with the load (%), the slowest render (µs, of the
5805 µs a 256-frame block has), late renders, renders, the bench flag and the time each part of the engine
took (sequencer, drums, tracks 2–4, sends, master, the rest), each as five 7-bit groups; `F0 7D 4C 34 03 F7`
clears them. With a bench build, `F0 7D 4C 34 02 k F7` loads test song k (0: three tracks of four-note
Juno chords and every drum; 1: the first demo; 2: the heaviest keys and every drum on every step; 3: three
mono leads).

On macOS with podman instead of Docker, put a `docker` script that runs `exec podman "$@"` first
on your PATH. `EN_JOBS` (default 4) limits parallel compiles: a podman machine drops
connections when many containers start at once.

Build option: `EN_CDC=1` adds Felucca's USB serial function (off by default: one plain
MIDI interface).

The build also generates `build/gen/` (the font). It fails if a soft-double routine is linked
(a `double` crept in) and checks the image, RAM and pool sizes.

## Tests

```
tests/run_tests.sh
```

Runs, on the build machine: the maths library against libm; the instrument (`tests/host/lcd4_test.c`:
the settings and patterns and their validation, the demos' levels, tempo and length, pattern switching,
chains and chords and following them, ratchets and swing, every punch-in effect, live writing (notes,
chords, lengths, slides, drum hits), MIDI clock in and out, mutes, every kit's every part and every sound at
its knobs' extremes); flash storage; Felucca's update-path tests against `build/lcd4.fwsc`; and the whole
app in the simulator (`tests/scenarios/*.lcd`), with its screenshots and audio in `build/scenarios/`.

`host/build_host.sh` builds the simulator alone; `build/host/lcd4_host SCRIPT OUTDIR` runs one
script (the command list is at the top of `host/lcd4_host.c`).

- `build/host/lcd4_host tests/demo.lcd build/demo`: the four demo songs, as WAV files (not part of the
  tests).
- `tools/manual_shots.sh`: the manual's screenshots (`tests/manual.lcd`) into `docs/img`, and the panel
  drawing (`tools/gen_manual_figures.py`).

## Install

From the command line (needs `pip3 install mido python-rtmidi`):

```
python3 tools/fm1_install.py build/lcd4.fwsc
python3 tools/fm1_install.py --info          # identity of the connected FM-1
```

Installing firmware is at your own risk. Hold OCT− and OCT+ for 5 seconds for Felucca's update
mode. If the FM-1 no longer starts but reaches the chip's update mode (4C4A:8057 on USB),
`bash tools/fm1_rescue.sh` puts stock firmware back from a Mac; otherwise recovery needs
[FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

## On GitHub: builds, releases, the site

Three workflows in `.github/workflows/` (they work the same in a fork):

- **Build and test** (every push and pull request): the toolchain and the SDK files fetched (and cached), the
  firmware built, `tests/run_tests.sh` run (with the browser build), the package kept as an artifact.
- **Release** (a tag `vX.Y.Z`): the tests, then `./build.sh --release X.Y.Z` (identity `FM-1_4XXYYZZ`, the
  version on the FM-1's screen), and a GitHub release with `lcd4-X.Y.Z.fwsc`, its notes taken from
  `CHANGELOG.md`.
- **Pages** (after a release, or by hand): the site for the latest release on GitHub Pages: the landing page,
  the browser emulator, the web installer with the release's package, the manual, and the complete source as
  a tarball (as the GPL asks). In a fork, set **Settings → Pages → Source** to **GitHub Actions** first.

To release: add the version's section to `CHANGELOG.md`, then `git tag vX.Y.Z && git push origin vX.Y.Z`.

The same by hand:

1. `./build.sh --release X.Y.Z` (the identity, FM-1_4XXYYZZ, is what the installer checks; IND-1's is
   FM-1_6..., AMB-1's FM-1_7..., X0X's FM-1_9...).
2. `web/emu/build.sh` (needs Emscripten) for the browser emulator.
3. `[LCD4_REPO=https://...] python3 tools/make_pages.py build/lcd4-X.Y.Z.fwsc X.Y.Z OUT_DIR`: the site in a
   static folder, for any web server. The web installer needs it served over HTTPS (Web MIDI). The
   manual's source is `docs/MANUAL.md` (`tools/manual_html.py` turns it into the page).
4. `tools/readme_gif.py` remakes the README's animation, `tools/manual_shots.sh` the manual's screenshots.

## On the FM-1: how it's doing

`python3 tools/fm1_status.py load` asks a connected FM-1 running LCD-4 for its audio load and where the
time goes; `play` for what it's playing, `store` for what's saved, `clock` to play it from the computer by
MIDI clock while measuring. It only asks; it never changes anything on the FM-1.
