# -*- coding: utf-8 -*-
"""Replace the 4 color-palette panel icons with Krita Breeze-dark assets
(+/- drawn from Breeze geometry for a matching pair)."""
import io, re

KRITA = r"C:\Users\zp122\Documents\trae_projects\ceshi\krita\krita\pics\svg"
PENCIL = r"C:\Users\zp122\Documents\trae_projects\ceshi\pencil\app\data\icons\themes\playful\misc"
LINE = "#E8E8EA"

# 1) add / remove color: clean Breeze-style +/- pair (16x16, sharp corners)
plus = ('<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">'
        '<path fill="%s" d="M7 2h2v5h5v2H9v5H7V9H2V7h5z"/></svg>\n' % LINE)
minus = ('<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">'
         '<path fill="%s" d="M2 7h12v2H2z"/></svg>\n' % LINE)

io.open(PENCIL + r"\add-color.svg", "w", encoding="utf-8", newline="\n").write(plus)
io.open(PENCIL + r"\remove-color.svg", "w", encoding="utf-8", newline="\n").write(minus)

# 2) color-dialog / more-options from Krita, tinted to theme color
def tint(s):
    s = re.sub(r'fill="#[0-9a-fA-F]{3,8}"', 'fill="%s"' % LINE, s)
    s = re.sub(r'stroke="#[0-9a-fA-F]{3,8}"', 'stroke="%s"' % LINE, s)
    s = re.sub(r'(fill|stroke)\s*:\s*#[0-9a-fA-F]{3,8}', lambda m: "%s: %s" % (m.group(1), LINE), s)
    s = re.sub(r'stop-color="#[0-9a-fA-F]{3,8}"', 'stop-color="%s"' % LINE, s)
    return s

for src, dst in [("dark_extended_color_selector.svg", "color-dialog.svg"),
                 ("dark_hamburger_menu_dots.svg", "more-options.svg")]:
    s = io.open(KRITA + "\\" + src, encoding="utf-8").read()
    io.open(PENCIL + "\\" + dst, "w", encoding="utf-8", newline="\n").write(tint(s))
    print("done:", dst)
