#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""docs/img/lcd4.gif: the README's animation, from the simulator. The first demo plays while the tracks and a
few sounds go by, a frame every 100 ms, each LCD pixel drawn as 2 x 2.

  tools/readme_gif.py        (builds the simulator first; needs Pillow)"""
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

SRC = Path(__file__).resolve().parents[1]
STEP_MS = 100
# what happens, and for how long: (commands before, frames)
PLAN = [
    (["tap PLAY", "wait 600", "turn ALGO -1"], 14),      # the 808: Pip at the kit
    (["turn ALGO 1"], 10),                               # Acid: the lab
    (["turn ALGO 1"], 10),                               # Poly: the sunset
    (["turn ALGO 1"], 10),                               # Arcade: the platform game
    (["turn PRESET -1"], 9),                             # Scream: the monster
    (["turn PRESET -1"], 9),                             # Sync: the rocket
    (["turn PRESET -9"], 9),                             # FM bass: the pinball table
]


def main():
    subprocess.run(["sh", str(SRC / "host" / "build_host.sh")], check=True, capture_output=True)
    with tempfile.TemporaryDirectory() as tmp:
        lines, n = ["wait 400"], 0
        for cmds, frames in PLAN:
            lines += cmds
            for _ in range(frames):
                lines += [f"wait {STEP_MS}", f"shot f{n:03d}.png"]
                n += 1
        script = Path(tmp) / "gif.lcd"
        script.write_text("\n".join(lines) + "\n")
        subprocess.run([str(SRC / "build" / "host" / "lcd4_host"), str(script), tmp], check=True, capture_output=True)
        frames = [Image.open(Path(tmp) / f"f{i:03d}.png").convert("RGB").resize((480, 480), Image.NEAREST)
                  for i in range(n)]
    pal = frames[0].quantize(colors=64, method=Image.Quantize.MEDIANCUT)
    out = [f.quantize(palette=pal, dither=Image.Dither.NONE) for f in frames]
    dest = SRC / "docs" / "img" / "lcd4.gif"
    out[0].save(dest, save_all=True, append_images=out[1:], duration=STEP_MS, loop=0, optimize=True)
    print(f"{dest}: {n} frames, {dest.stat().st_size // 1024} KB")


if __name__ == "__main__":
    sys.exit(main())
