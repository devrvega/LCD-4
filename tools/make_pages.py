#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make the LCD-4 site (a static folder: GitHub Pages or any web server serves it as it is):

  index.html                  what LCD-4 is, and the ways in: try, install, manual, download, source
  img/                        screenshots (docs/img)
  emu/                        LCD-4 in the browser (build/emu, from web/emu/build.sh)
  install/index.html          the web installer (web/lcd4_installer.html, with fm1pkg.js, fm1ota.js and
                              the package's metadata inlined; Chrome or Edge, Web MIDI)
  manual/index.html, img/     the manual (tools/manual_html.py, from docs/MANUAL.md)
  firmware/lcd4-VERSION.fwsc   the package the installer writes; also the download
  source/                     the complete source (GPL-3.0: whoever gets the firmware gets the source),
                              as a tarball of this tree; and LCD4_REPO, if set, the repository's address

  [LCD4_REPO=https://...] tools/make_pages.py build/lcd4-0.1.fwsc 0.1 OUT_DIR

The package must be a release build (./build.sh --release X.Y): its identity, FM-1_4XXYYZZ, is what
the installer checks the download against and what the FM-1 reports after the install."""
import html
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SRC / "web"))
from make_site import product_of, strip_module  # noqa: E402  (Felucca's: the package format)

REPO = os.environ.get("LCD4_REPO", "")

LANDING = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>LCD-4</title>
<meta name="description" content="A four-track pocket groovebox for the M-VAVE FM-1: a drum track with four kits, three synth tracks with sixteen circuit-modelled sounds, 16-step patterns, chains, chords and punch-in effects, on a screen of little LCD games.">
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'%3E%3Crect width='16' height='16' rx='3' fill='%23c5cdb3'/%3E%3Crect x='3' y='4' width='3' height='8' fill='%231e231d'/%3E%3Crect x='8' y='6' width='5' height='3' fill='%231e231d'/%3E%3C/svg%3E">
<style>
/* LCD-4's colours: a dark body, the LCD's grey-green, the four tracks' orange, cyan, yellow and pink */
:root { --paper: #161619; --sheet: #26262b; --ink: #eae8e0; --muted: #9c9c98; --lcd: #c5cdb3; --lcdink: #1e231d; --rule: #3a3a40;
        --t1: #ff7a28; --t2: #38c4e8; --t3: #f6cc28; --t4: #ec54a0; color-scheme: dark; }
body { background: var(--paper); color: var(--ink); margin: 0; padding: 40px 16px 64px; font: 17px/1.6 "Helvetica Neue", Helvetica, Arial, sans-serif; }
main { max-width: 46rem; margin: 0 auto; }
h1 { font-size: 3rem; font-weight: 700; letter-spacing: .04em; margin: 0; display: inline-block; background: var(--lcd); color: var(--lcdink);
     padding: .1em .45em; border-radius: 6px; box-shadow: 0 0 0 3px #5c6054; }
.stripes { display: flex; gap: 6px; margin: .9rem 0 0; } .stripes span { width: 44px; height: 7px; border-radius: 2px; }
.sub { color: var(--muted); margin: .8rem 0 1.4rem; font-size: 1.1rem; }
.lede { font-size: 1.12rem; }
.shots { display: flex; flex-wrap: wrap; gap: 12px; margin: 1.6rem 0; }
.shots img { width: calc((100% - 24px) / 3); min-width: 150px; height: auto; image-rendering: pixelated; border-radius: 6px; }
h2 { font-size: 1.15rem; margin: 2.2rem 0 .6rem; padding-top: .6rem; border-top: 1px solid var(--rule); }
.ways { display: grid; grid-template-columns: repeat(auto-fit, minmax(13rem, 1fr)); gap: 12px; margin: 1.6rem 0; }
.ways a { display: block; padding: .8rem 1rem; background: var(--sheet); border: 1px solid var(--rule); border-radius: 8px; text-decoration: none; color: var(--ink); }
.ways a:first-child { background: var(--lcd); color: var(--lcdink); }
.ways a strong { display: block; } .ways a span { display: block; font-size: .9rem; opacity: .8; }
.ways a:hover, .ways a:focus-visible { border-color: var(--t2); outline: none; }
.v { font-style: normal; font-weight: 400; opacity: .8; }
ul.tracks { padding-left: 1.2rem; } ul.tracks li { margin: .4rem 0; }
ul.tracks li:nth-child(1)::marker { color: var(--t1); } ul.tracks li:nth-child(2)::marker { color: var(--t2); }
ul.tracks li:nth-child(3)::marker { color: var(--t3); } ul.tracks li:nth-child(4)::marker { color: var(--t4); }
.small { color: var(--muted); font-size: .9rem; }
a { color: var(--ink); text-decoration-color: var(--t2); text-decoration-thickness: 2px; text-underline-offset: 3px; }
code { font-size: .88em; }
</style>
</head>
<body>
<main>
<h1>LCD-4</h1>
<div class="stripes"><span style="background:var(--t1)"></span><span style="background:var(--t2)"></span><span style="background:var(--t3)"></span><span style="background:var(--t4)"></span></div>
<p class="sub">A pocket groovebox for the M-VAVE FM-1</p>
<p class="lede">Four tracks: a drum machine and three synths, in 16-step patterns you chain together, with chords the
synths follow and sixteen punch-in effects, after the little synths that live in a calculator's body. Every sound is
modelled on a classic's circuit, and each one has its own little LCD game on the screen that moves with the music.</p>
<div class="shots">
  <img src="img/screen-drums.png" width="240" height="240" alt="LCD-4's screen: the 808 kit, Pip the drummer at the kit, the bass drum's steps">
  <img src="img/screen-acid.png" width="240" height="240" alt="LCD-4's screen: the Acid sound, a lab whose flasks bubble on every note">
  <img src="img/screen-arcade.png" width="240" height="240" alt="LCD-4's screen: the Arcade sound, a platform game">
</div>
<nav class="ways" aria-label="Get LCD-4">
  <a href="emu/"><strong>Try it in the browser</strong><span>The same code the FM-1 runs, with sound. Mouse, touch or keyboard; no FM-1 needed.</span></a>
  <a href="install/"><strong>Install <em class="v">v__VERSION__</em></strong><span>From Chrome or Edge, with the FM-1 connected by USB. Nothing to install on the computer.</span></a>
  <a href="manual/"><strong>Manual</strong><span>Getting started in a few minutes, then everything else.</span></a>
  <a href="firmware/__PKG__"><strong>Download <em class="v">__PKG__</em></strong><span>For the command-line installer: <code>python3 tools/fm1_install.py __PKG__</code></span></a>
  <a href="source/"><strong>Source</strong><span>GPL-3.0. Built on Felucca by Hügelton Instruments, by way of X0X, AMB-1 and IND-1.</span></a>
</nav>

<h2>The tracks</h2>
<ul class="tracks">
  <li><b>Drums.</b> Four kits of eleven parts on the black keys: an 808, a 909, an arcade console's (pulse kicks, noise snares, blips, zaps and a coin), and a rhythm box (a CR-78 and a 606). The white keys are the part's steps, as on X0X.</li>
  <li><b>Basses.</b> Acid (a TB-303, with its accents and slides), Mono (SH-101), Fat (Minimoog), Boom (an 808 sub), FM bass (DX7), Reese.</li>
  <li><b>Keys.</b> Poly (a Juno-60 and its chorus), Brass (Prophet-5 / OB-Xa), E.Piano (DX7), Organ (the M1's house organ), Strings (Solina, its ensemble), Bells.</li>
  <li><b>Leads.</b> Sync, Scream (MS-20), Arcade (NES and Game Boy channels, a tracker's effects) and Arp (chip arpeggios). Any synth track loads any sound.</li>
</ul>
<p>Sixteen patterns, chains of up to 64; sixteen chords and chains of them; effects held or written onto the steps;
parameter locks, ratchets, slides and swing; writing live or step by step. Four demo songs to start from.</p>
<p class="small">To go back to the official firmware, use the installer's "Back to the stock firmware", or
M-VAVE's updater, M-UPGRADE, from <a href="https://www.m-vave.com/download">m-vave.com/download</a>.
M-VAVE and FM-1 are trademarks of their owners. LCD-4 is independent: it is not affiliated with M-VAVE, Teenage
Engineering, Nintendo or the makers of the instruments its sounds are modelled on, and its art is its own.</p>
</main>
</body>
</html>
"""

SOURCE = """<!doctype html>
<html lang="en">
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>LCD-4 source</title>
<style>body { background: #161619; color: #eae8e0; font: 17px/1.6 "Helvetica Neue", Helvetica, Arial, sans-serif; margin: 48px 16px; }
main { max-width: 42rem; margin: 0 auto; } a { color: #eae8e0; text-decoration-color: #38c4e8; text-decoration-thickness: 2px; }</style></head>
<body><main><h1>LCD-4 source</h1>
<p>LCD-4 is free software under the GNU General Public License, version 3 only. The complete source of v__VERSION__:
<a href="__TAR__">__TAR__</a>.__REPOLINE__</p>
<p><a href="../">Back to LCD-4</a></p></main></body></html>
"""


def source_tarball(dest, version):
    """the tree as it is (no build output, no caches): what the GPL asks to give with the firmware"""
    name = f"lcd-4-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}-src"
    skip = {"build", "__pycache__", ".git", ".DS_Store", ".serena"}

    def keep(ti):
        return None if set(Path(ti.name).parts) & skip else ti
    with tarfile.open(dest / f"{name}.tar.gz", "w:gz") as t:
        t.add(SRC, arcname=name, filter=keep)
    return f"{name}.tar.gz"


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_4\d{6}", product):
        raise SystemExit(f"{pkg}: identity {product!r}: make a release build (./build.sh --release X.Y)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no update loader in it")
    name = f"lcd4-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    if out.exists():
        shutil.rmtree(out)
    for d in ("install", "manual", "firmware", "source"):
        (out / d).mkdir(parents=True)
    shutil.copy(pkg, out / "firmware" / name)
    # the installer
    page = (SRC / "web" / "lcd4_installer.html").read_text(encoding="utf-8")
    lib = strip_module((SRC / "web" / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((SRC / "web" / "fm1ota.js").read_text(encoding="utf-8"))
    meta = json.dumps({"version": version, "product": product, "pkg": "../firmware/" + name})
    for mark in ("/*LIB*/", "/*META*/"):
        if page.count(mark) != 1:
            raise SystemExit(f"lcd4_installer.html must contain {mark} once")
    (out / "install" / "index.html").write_text(page.replace("/*LIB*/", lib).replace("/*META*/", meta), encoding="utf-8")
    # the manual (a complete page; its images beside it)
    subprocess.run([sys.executable, str(SRC / "tools" / "manual_html.py"), str(SRC / "docs" / "MANUAL.md"),
                    str(out / "manual" / "index.html")], check=True, capture_output=True)
    shutil.copytree(SRC / "docs" / "img", out / "manual" / "img")
    # the landing page and its screenshots
    shutil.copytree(SRC / "docs" / "img", out / "img")
    (out / "index.html").write_text(LANDING.replace("__VERSION__", html.escape(version)).replace("__PKG__", name),
                                    encoding="utf-8")
    # the source, for the GPL
    tar = source_tarball(out / "source", version)
    repo = f' The repository: <a href="{html.escape(REPO)}">{html.escape(REPO)}</a>.' if REPO else ""
    (out / "source" / "index.html").write_text(SOURCE.replace("__VERSION__", html.escape(version)).replace("__TAR__", tar)
                                               .replace("__REPOLINE__", repo), encoding="utf-8")
    emu = SRC / "build" / "emu"
    if not (emu / "lcd4.wasm").exists():
        raise SystemExit("no build/emu/lcd4.wasm: run web/emu/build.sh (needs Emscripten)")
    shutil.copytree(emu, out / "emu")
    (out / ".nojekyll").write_text("")
    print(f"site: {out}: index.html, install/ ({product}), manual/, emu/, firmware/{name} ({len(raw)} B)")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
