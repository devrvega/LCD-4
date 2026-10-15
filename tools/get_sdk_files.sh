#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The three files of JieLi's AC79 SDK (Apache-2.0) the package needs, without cloning the whole SDK:
# cpu/wl82/tools/{uboot.boot, cfg_tool.bin, cfg/eq_cfg_hw.bin} of AC79NN_SDK_V1.2.1_2023-12-13, from JieLi's
# own repository (gitee), or a GitHub mirror of it when gitee can't be reached. Each file is kept only if its
# SHA-256 is the one tools/build.py expects, wherever it came from.
#   tools/get_sdk_files.sh [DEST]      (default: ~/fw-AC79_AIoT_SDK; then AC79_SDK=DEST ./build.sh)
set -e
DEST="${1:-$HOME/fw-AC79_AIoT_SDK}"
TAG=AC79NN_SDK_V1.2.1_2023-12-13
SOURCES="https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/raw/$TAG https://raw.githubusercontent.com/amitv87/fw-AC79_AIoT_SDK/$TAG"
sha() { if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1; else shasum -a 256 "$1" | cut -d' ' -f1; fi; }
get() {   # get REL SHA256
    out="$DEST/cpu/wl82/tools/$1"
    mkdir -p "$(dirname "$out")"
    if [ -f "$out" ] && [ "$(sha "$out")" = "$2" ]; then
        echo "  ok   $1 (already there)"
        return 0
    fi
    for base in $SOURCES; do
        if curl -fsSL --retry 2 --max-time 60 "$base/cpu/wl82/tools/$1" -o "$out.part" 2>/dev/null &&
           [ "$(sha "$out.part")" = "$2" ]; then
            mv "$out.part" "$out"
            echo "  ok   $1 (from $base)"
            return 0
        fi
    done
    rm -f "$out.part"
    echo "  FAIL $1: no source gave the expected file (SHA-256 $2)"
    return 1
}
get uboot.boot 4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4
get cfg_tool.bin 276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b
get cfg/eq_cfg_hw.bin 41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d
echo "AC79_SDK=$DEST"
