# -*- coding: utf-8 -*-
"""Bind the white paper rect to the camera frame (follows zoom/pan)."""
import io

def patch(path, pairs):
    s = io.open(path, encoding='utf-8').read()
    for old, new in pairs:
        assert s.count(old) == 1, (path, old[:70])
        s = s.replace(old, new)
    io.open(path, 'w', encoding='utf-8', newline='').write(s)
    print('patched', path.split('\\')[-1] if '\\' in path else path.split('/')[-1])

R = r'C:\Users\zp122\Documents\trae_projects\ceshi\pencil'

# ---- backgroundwidget.h: editor pointer ----
patch(R + r'\core_lib\src\interface\backgroundwidget.h', [
('class PreferenceManager;', 'class PreferenceManager;\nclass Editor;'),
('    void init(PreferenceManager* prefs);',
 '    void init(PreferenceManager* prefs);\n    void setEditor(Editor* editor) { mEditor = editor; }'),
])

# read header to find member section anchor
h = R + r'\core_lib\src\interface\backgroundwidget.h'
s = io.open(h, encoding='utf-8').read()
assert 'PreferenceManager* mPrefs' in s
s = s.replace('PreferenceManager* mPrefs', 'PreferenceManager* mPrefs = nullptr;\n    Editor* mEditor = nullptr;', 1)
io.open(h, 'w', encoding='utf-8', newline='').write(s)
print('header members OK')

# ---- backgroundwidget.cpp: paper = camera rect mapped to screen ----
patch(R + r'\core_lib\src\interface\backgroundwidget.cpp', [
('#include "backgroundwidget.h"',
 '''#include "backgroundwidget.h"

#include "editor.h"
#include "layermanager.h"
#include "viewmanager.h"
#include "layercamera.h"'''),
('''    const int margin = 24;
    const QRect workspace = r;
    const QRect paper = r.adjusted(margin, margin, -margin, -margin);

    painter.setClipRegion(QRegion(workspace).subtracted(QRegion(paper)));''',
 '''    // Paper follows the camera frame: the camera rect in document space,
    // mapped through the camera transform and the view transform, so it
    // scales/pans together with the camera border instead of staying fixed.
    QPolygonF paperPoly;
    bool havePaper = false;
    if (mEditor != nullptr && mEditor->layers() != nullptr)
    {
        Layer* l = mEditor->layers()->getCameraLayerBelow(mEditor->currentLayerIndex());
        LayerCamera* cam = dynamic_cast<LayerCamera*>(l);
        if (cam != nullptr)
        {
            const QTransform camT = cam->getViewAtFrame(mEditor->currentFrame()).inverted();
            const QPolygonF camPoly = camT.map(QPolygonF(QRectF(cam->getViewRect())));
            for (const QPointF& pnt : camPoly)
                paperPoly << mEditor->view()->mapCanvasToScreen(pnt);
            havePaper = paperPoly.size() == 4;
        }
    }
    const QRect paper = havePaper ? paperPoly.boundingRect().toAlignedRect()
                                  : r.adjusted(24, 24, -24, -24);
    const QRect workspace = r;

    painter.setClipRegion(QRegion(workspace).subtracted(QRegion(paper)));'''),
('''    // clean light paper for drawing (strokes stay readable on it), rounded
    painter.setClipRect(event->rect());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0xFF, 0xFF, 0xFF));
    painter.drawRoundedRect(paper, 12.0, 12.0);
    painter.setRenderHint(QPainter::Antialiasing, false);''',
 '''    // clean light paper for drawing (strokes stay readable on it)
    painter.setClipRect(event->rect());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0xFF, 0xFF, 0xFF));
    if (havePaper)
    {
        // camera-mapped paper: rounded when axis-aligned, plain polygon otherwise
        const QPolygonF::const_iterator pts = paperPoly.cbegin();
        Q_UNUSED(pts);
        const QRectF b = paperPoly.boundingRect();
        const bool axisAligned = qFuzzyCompare(paperPoly.at(0).x(), b.left()) && qFuzzyCompare(paperPoly.at(0).y(), b.top())
            && qFuzzyCompare(paperPoly.at(1).x(), b.right()) && qFuzzyCompare(paperPoly.at(1).y(), b.top())
            && qFuzzyCompare(paperPoly.at(2).x(), b.right()) && qFuzzyCompare(paperPoly.at(2).y(), b.bottom())
            && qFuzzyCompare(paperPoly.at(3).x(), b.left()) && qFuzzyCompare(paperPoly.at(3).y(), b.bottom());
        if (axisAligned)
            painter.drawRoundedRect(b, 12.0, 12.0);
        else
            painter.drawPolygon(paperPoly);
    }
    else
    {
        painter.drawRoundedRect(paper, 12.0, 12.0);
    }
    painter.setRenderHint(QPainter::Antialiasing, false);'''),
])

# ---- mainwindow2.cpp: give editor to the paper + repaint on view change ----
patch(R + r'\app\src\mainwindow2.cpp', [
('''    mEditor = new Editor(this);
    mEditor->setScribbleArea(ui->scribbleArea);
    mEditor->init();''',
 '''    mEditor = new Editor(this);
    mEditor->setScribbleArea(ui->scribbleArea);
    mEditor->init();
    ui->background->setEditor(mEditor);
    // paper follows the camera frame: repaint whenever the view zooms/pans
    connect(mEditor->view(), &ViewManager::viewChanged, ui->background, qOverload<>(&QWidget::update));'''),
])
print('ALL DONE')
