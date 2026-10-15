#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The manual's screenshots, from the simulator (tests/manual.lcd), into docs/img; then the panel figure
# (tools/gen_manual_figures.py) with the drums' screen in it.
#   tools/manual_shots.sh
set -e
cd "$(dirname "$0")/.."
sh host/build_host.sh >/dev/null
mkdir -p build/manual
build/host/lcd4_host tests/manual.lcd build/manual >/dev/null
cp build/manual/screen-*.png docs/img/
python3 -c 'import sys; from PIL import Image; [Image.open(f).save(f, optimize=True) for f in sys.argv[1:]]' docs/img/screen-*.png
python3 tools/gen_manual_figures.py docs/img/screen-drums.png
ls docs/img
