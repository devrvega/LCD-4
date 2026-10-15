# LCD-4 licensing

LCD-4 is free software under the GNU General Public License, version 3 only
(`GPL-3.0-only`, full text in `LICENSE`). It is built on IND-1 and AMB-1, which are built on Felucca's
platform layer (through X0X and FoMni), and keeps their licence. If you distribute LCD-4, or firmware derived from it, you
must give your recipients its complete corresponding source under the same licence (`tools/make_pages.py`
puts a tarball of the source on the site it makes, next to the firmware).

## Where the code comes from

| What | Origin | Licence |
| --- | --- | --- |
| Platform: `firmware/hal/`, `firmware/loader/`, `firmware/src/{libc,lcd,gfx,usb,storage,ota,midi_uart}.c`, `tools/` (build, package, install, rescue), `web/fm1*.js` | [Felucca](https://github.com/hugelton/Felucca), Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, as changed for [X0X](https://github.com/charlesvestal/fm1-x0x), FoMni and [AMB-1](https://github.com/charlesvestal/fm1-amb), then IND-1 and LCD-4 | GPL-3.0-only |
| `firmware/src/app/{panel,plat_fm1,main_fm1,project}.c`, `firmware/src/app/ui.c` (its input, knob and drawing code), `host/lcd4_host.c`, `web/emu/`, `web/lcd4_installer.html`, `tools/{make_pages,manual_html,gen_manual_figures}.py` | adapted from IND-1 and AMB-1 (themselves from X0X, from Felucca's `panel.c`, `main.c`, `audio.c`) | GPL-3.0-only |
| In `firmware/src/dsp/`: Dattorro's plate reverb (`lcd4_fx.c`, rearranged), the ping-pong delay, the command queue, the clock and transport, the sine table, the noise, the polyBLEP saw and the state-variable filter | from IND-1 and AMB-1 (the plate from [FoMni](https://github.com/charlesvestal/fm1-fomni)) | GPL-3.0-only |
| The idea of a browser emulator for FM-1 firmware (`web/emu/`) | [Groove OS](https://www.groove-os.com/) by Peter Gombos, whose site runs its firmware in the browser; no code from it | credit only |
| Everything else in `firmware/src/{app,dsp}` (the drums, the sixteen sounds, the sequencer, the effects, the scenes and their art), `tests/` | LCD-4 | GPL-3.0-only |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Barlow Semi Condensed (The Barlow Project Authors), the UI face | SIL OFL 1.1 | `assets/fonts/BarlowSemiCondensed-*.ttf`, `assets/fonts/Barlow-OFL.txt` |
| Terminus (Dimitar Toshkov Zhekov), an alternative font set | SIL OFL 1.1 | `assets/fonts/ter-u*.bdf`, `assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments.
"M-VAVE" and "FM-1" are trademarks of their respective owners, as are the names of the instruments
LCD-4's sounds are modelled on (Roland, Yamaha, Korg, Moog, Sequential, Oberheim, ARP, PPG), "pocket
operator" (Teenage Engineering) and "Game & Watch" (Nintendo). LCD-4 is independent firmware; it is not
affiliated with, endorsed by or supported by any of them. The names are used only to say what a sound is
modelled on; LCD-4 contains no samples, code or artwork of theirs, and its scenes and characters are its own.

## Radio

LCD-4 never enables the Bluetooth / Wi-Fi radio of the hardware.
