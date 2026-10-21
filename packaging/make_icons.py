#!/usr/bin/env python3
"""Renders packaging/phong.svg into the icons of the desktop builds.

phong.png for Linux and the window, phong.ico for Windows and phong.icns
for macOS. Chrome draws the SVG, its blur filters are what the logo is
made of, Pillow writes the rest. Run it again after changing phong.svg.

    python3 packaging/make_icons.py [path to chrome]
"""

import pathlib
import shutil
import subprocess
import sys
import tempfile

from PIL import Image, ImageFilter

here = pathlib.Path(__file__).resolve().parent


def render(size):
    chrome = sys.argv[1] if len(sys.argv) > 1 else next(
        filter(None, map(shutil.which, ["google-chrome", "chromium", "chrome"])), None)
    if not chrome:
        sys.exit("No Chrome found, pass its path")
    with tempfile.TemporaryDirectory() as tmp:
        page = pathlib.Path(tmp, "icon.html")
        page.write_text("<!doctype html><style>html,body{margin:0;background:transparent}"
                        "img{display:block;width:100vw;height:100vh}</style>"
                        f"<img src='{(here / 'phong.svg').as_uri()}'>")
        shot = pathlib.Path(tmp, "icon.png")
        subprocess.run([chrome, "--headless=new", "--disable-gpu", "--hide-scrollbars",
                        "--default-background-color=00000000", f"--window-size={size},{size}",
                        f"--screenshot={shot}", page.as_uri()],
                       check=True, capture_output=True)
        return Image.open(shot).convert("RGBA")


icon = render(1024)

icon.resize((512, 512), Image.LANCZOS).save(here / "phong.png")

icon.save(here / "phong.ico", sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])

# macOS icons keep a margin around the tile and cast a shadow, an icon
# filling the whole square looks too big in the dock next to the others
tile = icon.resize((824, 824), Image.LANCZOS)
shadow = Image.new("RGBA", (1024, 1024))
shadow.paste((0, 0, 0, 110), (100, 112), tile.getchannel("A"))
mac = shadow.filter(ImageFilter.GaussianBlur(14))
mac.alpha_composite(tile, (100, 100))
mac.save(here / "phong.icns")
