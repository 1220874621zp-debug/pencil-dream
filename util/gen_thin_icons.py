# -*- coding: utf-8 -*-
"""Generate thin-line icons (overwrite playful theme) + monochrome-tint the rest."""
import os, re, io

ROOT = r"C:\Users\zp122\Documents\trae_projects\ceshi\pencil\app\data\icons\themes\playful"
LINE = "#E8E8EA"
ACCENT = "#E8385A"

def svg(body, extra=""):
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="100%%" height="100%%" viewBox="0 0 22 22" '
            'fill="none" stroke="%s" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">%s</svg>\n' % (LINE, body))

S = 'stroke="%s"' % LINE
F = 'fill="%s" stroke="none"' % LINE
FA = 'fill="%s" stroke="none"' % ACCENT

icons = {
# ---- tools ----
"tool-pencil.svg": svg(
    '<line x1="13.2" y1="4.2" x2="18" y2="9"/>'
    '<line x1="4.5" y1="17.5" x2="5.6" y2="13.4" stroke-width="1.4"/>'
    '<path d="M5.6 13.4 L13.2 4.2 L18 9 L10.4 18.2 Z" stroke-width="1.4"/>'
    '<path d="M5.6 13.4 L10.4 18.2 L4.5 17.5 Z" %s/>' % F),
"tool-eraser.svg": svg(
    '<rect x="4" y="9" width="9.5" height="7" rx="1.5" transform="rotate(-45 8.75 12.5)"/>'
    '<line x1="10.2" y1="6.3" x2="15.7" y2="11.8"/>'
    '<line x1="4" y1="18.5" x2="18" y2="18.5"/>'),
"tool-select.svg": svg(
    '<rect x="4" y="5" width="14" height="12" rx="1" stroke-dasharray="3 2.4"/>'),
"tool-move.svg": svg(
    '<line x1="11" y1="4" x2="11" y2="18"/>'
    '<line x1="4" y1="11" x2="18" y2="11"/>'
    '<path d="M9 6 L11 4 L13 6 M9 16 L11 18 L13 16 M6 9 L4 11 L6 13 M16 9 L18 11 L16 13" stroke-width="1.4"/>'),
"tool-hand.svg": svg(
    '<path d="M8 11 V5.8 a1.1 1.1 0 0 1 2.2 0 V10 M10.2 10 V4.6 a1.1 1.1 0 0 1 2.2 0 V10 M12.4 10 V5.6 a1.1 1.1 0 0 1 2.2 0 V11.2"/>'
    '<path d="M15 11.2 c0-1 .8-1.6 1.6-1.2 .8 .4 1 .9 .8 1.8 l-.9 3.8 c-.4 1.7-1.8 2.8-3.6 2.8 h-2.6 c-1.4 0-2.4-.5-3.2-1.6 L5 13.6 c-.6-.9-.4-1.9 .5-2.4 .7-.4 1.6-.2 2.1 .5 l.4 .6" stroke-width="1.4"/>'),
"tool-pen.svg": svg(
    '<path d="M15.5 3.5 L18.5 6.5 L9 16 L5 17 L6 13 Z"/>'
    '<path d="M14 5 L17 8"/>'
    '<path d="M5 17 L6.6 15.4"/>'),
"tool-polyline.svg": svg(
    '<polyline points="4,16 8.5,8.5 13.5,13 18,5" stroke-width="1.5"/>'
    '<circle cx="4" cy="16" r="1.6" %s/><circle cx="8.5" cy="8.5" r="1.6" %s/>'
    '<circle cx="13.5" cy="13" r="1.6" %s/><circle cx="18" cy="5" r="1.6" %s/>' % (F, F, F, F)),
"tool-bucket.svg": svg(
    '<path d="M8 4 L15 4 L19 8 L11.5 15.5 a2.3 2.3 0 0 1-3.2 0 L4 11 Z"/>'
    '<path d="M15 4 L19 8"/>'
    '<path d="M17.2 13.2 c.9 1 .9 2.4 0 3.3 -.9 .9-2.3 .9-3.1 0 -.9-1-.9-2.4 0-3.3 .8-.9 2.2-.9 3.1 0 Z" %s/>' % F),
"tool-eyedropper.svg": svg(
    '<path d="M14.5 3.5 l4 4 -1.8 1.8 -.9 -.9 -7.3 7.3 -2.7 .7 -1.5 1.5 -1.3 -1.3 1.5 -1.5 .7 -2.7 7.3 -7.3 -.9 -.9 Z"/>'
    '<path d="M13.2 6.6 l2.2 2.2"/>'),
"tool-brush.svg": svg(
    '<path d="M15.5 3.5 c1.4 1.4 1.4 3 .4 4.4 l-6.4 6.4 -3.3 -3.3 6.4 -6.4 c1.4-1 3-1 4.4 -.4 Z" transform="rotate(45 11 9)"/>'
    '<path d="M7.2 12.2 c-1.6 .8-2.2 2-2.7 4.5 2.5-.5 3.7-1.1 4.5-2.7"/>'
    '<path d="M4.5 16.7 l1-1"/>'),
"tool-smudge.svg": svg(
    '<path d="M4 14 c2-4 4 2 6-2 s4 2 7-3" stroke-width="1.7"/>'
    '<circle cx="16.5" cy="6.5" r="1.8" %s/>' % F),
"tool-camera-move.svg": svg(
    '<rect x="6" y="6" width="10" height="10" rx="1.2"/>'
    '<path d="M11 2.5 v2 M11 17.5 v2 M2.5 11 h2 M17.5 11 h2" stroke-width="1.4"/>'
    '<path d="M9.7 3.7 L11 2.5 l1.3 1.2 M9.7 18.3 L11 19.5 l1.3-1.2 M3.7 9.7 L2.5 11 l1.2 1.3 M18.3 9.7 L19.5 11 l-1.2 1.3" stroke-width="1.2"/>'),
"tool-camera-rotate.svg": svg(
    '<rect x="6" y="6" width="10" height="10" rx="1.2"/>'
    '<path d="M17.5 5.5 a8 8 0 0 0-13-1" stroke-width="1.4"/>'
    '<path d="M4.5 2.5 l0 3 3 0" stroke-width="1.4"/>'
    '<path d="M4.5 16.5 a8 8 0 0 0 13 1" stroke-width="1.4"/>'
    '<path d="M17.5 19.5 l0-3 -3 0" stroke-width="1.4"/>'),
"tool-camera-scale.svg": svg(
    '<rect x="5" y="5" width="9" height="9" rx="1.2"/>'
    '<path d="M12.5 12.5 L19 6" stroke-width="1.4"/>'
    '<path d="M15.5 6 h3.5 v3.5" stroke-width="1.4"/>'),
# ---- controls ----
"control-play.svg": svg('<path d="M7 4.5 L17.5 11 L7 17.5 Z" %s/>' % F),
"control-stop.svg": svg('<rect x="5.5" y="5.5" width="11" height="11" rx="1.5" %s/>' % F),
"control-play-start.svg": svg(
    '<path d="M17.5 5 L9.5 11 L17.5 17 Z" %s/>' % F +
    '<line x1="5.5" y1="5" x2="5.5" y2="17" stroke-width="2"/>'),
"control-play-end.svg": svg(
    '<path d="M4.5 5 L12.5 11 L4.5 17 Z" %s/>' % F +
    '<line x1="16.5" y1="5" x2="16.5" y2="17" stroke-width="2"/>'),
"control-loop.svg": svg(
    '<path d="M6 13 a5 5 0 0 1 9.5-2.5 M16 9 a5 5 0 0 1-9.5 2.5" stroke-width="1.5"/>'
    '<path d="M15.5 6.5 v4 h-4 M6.5 15.5 v-4 h4" stroke-width="1.4"/>'),
"control-sound-enable.svg": svg(
    '<path d="M4 9 v4 h3 l4 3.5 V5.5 L7 9 Z"/>'
    '<path d="M13.5 8.5 a4 4 0 0 1 0 5 M15.5 6.5 a7 7 0 0 1 0 9" stroke-width="1.4"/>'),
"control-sound-scrub.svg": svg(
    '<path d="M4 9 v4 h3 l4 3.5 V5.5 L7 9 Z"/>'
    '<path d="M14 7.5 v7 M17 9.5 v3" stroke-width="1.6"/>'),
# ---- timeline cells & ops ----
"cell-bitmap.svg": svg(
    '<rect x="3.5" y="3.5" width="15" height="15" rx="2" stroke-width="1.4"/>'
    '<rect x="7" y="7" width="3" height="3" %s/><rect x="12" y="7" width="3" height="3" %s/>'
    '<rect x="7" y="12" width="3" height="3" %s/><rect x="12" y="12" width="3" height="3" %s/>' % (F, F, F, F)),
"cell-sound.svg": svg(
    '<path d="M9 5 v8.5" stroke-width="1.5"/>'
    '<circle cx="7" cy="14.2" r="2.2"/>'
    '<path d="M9 5 c3 .8 5 2 5 4.5" stroke-width="1.4"/>'
    '<path d="M9 8.5 c1.8 .5 3 1.3 3 2.8" stroke-width="1.4"/>'),
"cell-camera.svg": svg(
    '<rect x="3" y="6" width="11" height="10" rx="1.8"/>'
    '<path d="M14 10.5 L19 8 v6 L14 11.5 Z"/>'),
"frame-add.svg": svg(
    '<rect x="4" y="6" width="14" height="10" rx="1.5" stroke-width="1.4"/>'
    '<line x1="11" y1="8.5" x2="11" y2="13.5"/><line x1="8.5" y1="11" x2="13.5" y2="11"/>'),
"frame-remove.svg": svg(
    '<rect x="4" y="6" width="14" height="10" rx="1.5" stroke-width="1.4"/>'
    '<line x1="8.5" y1="11" x2="13.5" y2="11"/>'),
"frame-duplicate.svg": svg(
    '<rect x="3" y="3" width="10" height="8" rx="1.5" stroke-width="1.4"/>'
    '<rect x="9" y="9" width="10" height="8" rx="1.5" stroke-width="1.4" %s/>' % S.replace('stroke=', 'stroke=') ),
"layer-add.svg": svg(
    '<path d="M11 4 L19 8.5 11 13 3 8.5 Z" stroke-width="1.4"/>'
    '<path d="M5.5 11.5 L11 14.5 16.5 11.5" stroke-width="1.4"/>'
    '<line x1="17" y1="16" x2="17" y2="20" stroke-width="1.6"/><line x1="15" y1="18" x2="19" y2="18" stroke-width="1.6"/>'),
"layer-remove.svg": svg(
    '<path d="M11 4 L19 8.5 11 13 3 8.5 Z" stroke-width="1.4"/>'
    '<path d="M5.5 11.5 L11 14.5 16.5 11.5" stroke-width="1.4"/>'
    '<line x1="15" y1="18" x2="19" y2="18" stroke-width="1.6"/>'),
"layer-duplicate.svg": svg(
    '<path d="M8 3 L16 7.5 8 12 .5 7.5 Z" stroke-width="1.4" transform="translate(2.5,1)"/>'
    '<path d="M6 8.5 L14 13 6 17.5 -2 13 Z" stroke-width="1.4" transform="translate(5,3)"/>'),
}

def tint(content):
    # monochrome-tint: every concrete fill/stroke color -> light gray, keep opacity attrs
    content = re.sub(r'fill="#[0-9a-fA-F]{3,8}"', 'fill="%s"' % LINE, content)
    content = re.sub(r'stroke="#[0-9a-fA-F]{3,8}"', 'stroke="%s"' % LINE, content)
    return content

written, tinted = [], []
for rel, body in icons.items():
    p = os.path.join(ROOT, rel)
    # tool/frame/layer/cell/control files live in subfolders
    for sub in ("tools", "controls", "timeline"):
        cand = os.path.join(ROOT, sub, rel)
        if os.path.exists(cand):
            p = cand
            break
    else:
        p = os.path.join(ROOT, "tools", rel)
    io.open(p, "w", encoding="utf-8", newline="\n").write(body)
    written.append(os.path.relpath(p, ROOT))

# tint everything else
redrawn = set(os.path.basename(x) for x in written)
for dirpath, _, files in os.walk(ROOT):
    for fn in files:
        if not fn.endswith(".svg") or fn in redrawn:
            continue
        p = os.path.join(dirpath, fn)
        s = io.open(p, encoding="utf-8").read()
        s2 = tint(s)
        if s2 != s:
            io.open(p, "w", encoding="utf-8", newline="\n").write(s2)
            tinted.append(os.path.relpath(p, ROOT))

print("redrawn:", len(written))
print("tinted:", len(tinted))
for t in tinted[:10]:
    print("  ", t)
