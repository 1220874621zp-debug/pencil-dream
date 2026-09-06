# -*- coding: utf-8 -*-
"""1) Replace high-frequency icons with friction's Breeze SVGs.
2) Fix the 22 icons whose colors live in inline style="..." (missed by the
   earlier attribute-only tint)."""
import os, re, io

FRICTION = r"C:\Users\zp122\Documents\trae_projects\ceshi\friction\src\app\icons\hicolor\scalable\actions"
PENCIL = r"C:\Users\zp122\Documents\trae_projects\ceshi\pencil\app\data\icons\themes\playful"
LINE = "#E8E8EA"

# friction SVG -> playful target
REPL = {
    "document-new.svg":  "menubar/new.svg",
    "document-open.svg": "menubar/open.svg",
    "document-save.svg": "menubar/save.svg",
    "zoom_in.svg":       "menubar/zoom-in.svg",
    "zoom_out.svg":      "menubar/zoom-out.svg",
    "zoom_all.svg":      "menubar/view-reset.svg",
    "copy.svg":          "menubar/copy.svg",
    "cut.svg":           "menubar/cut.svg",
    "paste.svg":         "menubar/paste.svg",
    "select.svg":        "tools/tool-select.svg",
}

def unify(s):
    s = re.sub(r'(?:fill|stroke)="#[0-9a-fA-F]{3,8}"', lambda m: m.group(0).split('=')[0] + '="%s"' % LINE, s)
    return s

for src, dst in REPL.items():
    sp = os.path.join(FRICTION, src)
    dp = os.path.join(PENCIL, dst)
    body = io.open(sp, encoding="utf-8").read()
    io.open(dp, "w", encoding="utf-8", newline="\n").write(unify(body))
    print("replaced:", dst)

# --- pass 2: inline style= colors anywhere under playful (except freshly replaced) ---
replaced_files = set(REPL.values())
fixed = []
for dp_, _, fs in os.walk(PENCIL):
    for fn in fs:
        rel = os.path.relpath(os.path.join(dp_, fn), PENCIL).replace("\\", "/")
        if not fn.endswith(".svg") or rel in replaced_files:
            continue
        p = os.path.join(dp_, fn)
        s = io.open(p, encoding="utf-8", errors="replace").read()
        s2 = re.sub(r'(fill|stroke)\s*:\s*(#[0-9a-fA-F]{3,8}|rgb\([0-9,\s]+\)|red|blue|green|yellow|orange|purple|brown|pink|white|black)',
                    lambda m: "%s: %s" % (m.group(1), LINE), s)
        if s2 != s:
            io.open(p, "w", encoding="utf-8", newline="\n").write(s2)
            fixed.append(rel)

print("style-fixed:", len(fixed))
for f in fixed:
    print("  ", f)
