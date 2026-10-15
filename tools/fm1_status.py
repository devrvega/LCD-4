#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Ask a connected FM-1 running LCD-4 how it's doing, over USB-MIDI (LCD-4's read-only SysEx, F0 7D 4C 34 ..
F7, described in firmware/src/app/main_fm1.c). Needs mido with python-rtmidi. Nothing it sends changes the
FM-1's patterns or settings.

  tools/fm1_status.py load [SECONDS]   the audio load: %, the slowest render, late renders, and where the
                                       time goes (sequencer, drums, tracks 2-4, sends, master, the rest),
                                       measured over SECONDS (default 5); play something meanwhile
  tools/fm1_status.py play             what plays: notes counted a track, the pattern, the tempo, the step
  tools/fm1_status.py store            the saved objects: their lengths in flash, what is in them
  tools/fm1_status.py clock [SECONDS]  play LCD-4 from here: MIDI start, clock at 120, stop (default 6 s),
                                       and the load while it plays

A render has 5805 us (256 frames at 44.1 kHz); the FM-1 keeps up while the slowest stays under that."""
import sys
import time

import mido

HDR = [0x7D, 0x4C, 0x34]
PARTS = ["sequencer", "drums", "track 2", "track 3", "track 4", "sends", "master", "the rest"]


def r35(b, i):
    return sum(b[i + k] << (7 * k) for k in range(5))


def port_name():
    name = next((n for n in mido.get_output_names() if "FM-1" in n), None)
    if not name:
        sys.exit(f"fm1_status: no FM-1 MIDI port (ports: {mido.get_output_names()})")
    return name


def ask(i, o, cmd, args=(), reply=True, timeout=2.0):
    while i.poll():
        pass
    o.send(mido.Message("sysex", data=HDR + [cmd] + list(args)))
    if not reply:
        time.sleep(0.15)                    # the FM-1 takes one SysEx at a time
        return None
    end = time.time() + timeout
    while time.time() < end:
        m = i.poll()
        if m is not None and m.type == "sysex" and list(m.data[:4]) == HDR + [cmd]:
            return list(m.data[4:])
        time.sleep(0.002)
    sys.exit("fm1_status: no answer (is it LCD-4, and a version that answers this?)")


def load_report(d0, d1, secs):
    renders = max(r35(d1, 15) - r35(d0, 15), 1)
    us = [r35(d1, 25 + 5 * k) / 24.0 / renders for k in range(8)]
    print(f"load {r35(d1, 0)} %, slowest render {r35(d1, 5)} us of 5805, late {r35(d1, 10)}, "
          f"{renders} renders in {secs:.0f} s")
    print("per render: " + ", ".join(f"{p} {u:.0f} us" for p, u in zip(PARTS, us)) + f" = {sum(us):.0f} us")


def main():
    a = sys.argv[1:] or ["load"]
    with mido.open_input(port_name()) as i, mido.open_output(port_name()) as o:
        if a[0] in ("load", "clock"):
            secs = float(a[1]) if len(a) > 1 else (5.0 if a[0] == "load" else 6.0)
            ask(i, o, 3, reply=False)                       # the worst and the parts' times from now
            d0 = ask(i, o, 1)
            if a[0] == "clock":
                o.send(mido.Message("start"))
                t0, nxt = time.time(), time.time()
                while time.time() - t0 < secs:
                    if time.time() >= nxt:
                        o.send(mido.Message("clock"))
                        nxt += 60.0 / 120 / 24
                    time.sleep(0.001)
                d1 = ask(i, o, 1)
                o.send(mido.Message("stop"))
            else:
                time.sleep(secs)
                d1 = ask(i, o, 1)
            load_report(d0, d1, secs)
            st = r35(d1, 75)
            print(f"playing {st & 1}, on MIDI clock {st >> 1 & 1}, writing {st >> 2 & 1}; "
                  f"steps {r35(d1, 80)}; MIDI messages in {r35(d1, 85)}")
        elif a[0] == "play":
            d = ask(i, o, 7)
            v = [r35(d, 5 * k) for k in range(11)]
            print(f"notes played (counted mod 256): drums {v[0]}, track 2 {v[1]}, track 3 {v[2]}, track 4 {v[3]}")
            print(f"pattern P{v[4] + 1:02d} (selected P{(v[5] & 255) + 1:02d}, chain of {v[5] >> 8 & 255}, "
                  f"mutes {v[5] >> 16:04b}), its drum hits {v[6]}, its length {v[7] >> 8}")
            print(f"tempo {v[8] & 0xFFFF}, clock {'internal' if v[8] >> 16 else 'auto'}, effect {v[9] & 255}, "
                  f"step {(v[9] >> 8 & 255) + 1}, levels drums {v[10] & 255} track 2 {v[10] >> 8 & 255}")
        elif a[0] == "store":
            d = ask(i, o, 5)
            for k, name in enumerate(["settings", "panel", "patterns 1-4", "patterns 5-8", "patterns 9-12",
                                      "patterns 13-16"]):
                n, rc, info = r35(d, 15 * k) - 100, r35(d, 15 * k + 5) - 100, r35(d, 15 * k + 10)
                what = ""
                if k == 0 and n > 0:
                    what = f": tempo {info & 1023}, a chain of {info >> 10 & 1023}, {'valid' if info >> 20 else 'NOT valid'}"
                elif k >= 2 and n > 0:
                    what = f": valid {info & 15:04b}, drum hits {info >> 4 & 0xFFFF}"
                print(f"{name:15} {('%d B' % n) if n >= 0 else 'none':8} last save {'ok' if rc == 0 else rc}{what}")
        else:
            sys.exit(__doc__)


if __name__ == "__main__":
    main()
