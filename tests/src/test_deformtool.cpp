/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "catch.hpp"

#include "editor.h"
#include "object.h"
#include "layermanager.h"
#include "layerbitmap.h"
#include "bitmapimage.h"
#include "scribblearea.h"
#include "toolmanager.h"
#include "deformtool.h"

#include <QMouseEvent>
#include <QImage>

namespace
{
    // exposes the protected widget handlers so the test drives the real
    // event path (mMouseInUse tracking included)
    class TestScribbleArea : public ScribbleArea
    {
    public:
        using ScribbleArea::ScribbleArea;

        void testMousePress(const QPointF& pos)
        {
            QMouseEvent e(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            mousePressEvent(&e);
        }

        void testMouseMove(const QPointF& pos)
        {
            QMouseEvent e(QEvent::MouseMove, pos, pos, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            mouseMoveEvent(&e);
        }

        void testMouseRelease(const QPointF& pos)
        {
            QMouseEvent e(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            mouseReleaseEvent(&e);
        }
    };

    struct DeformHarness
    {
        Editor* editor = nullptr;
        ScribbleArea* scribbleArea = nullptr;
        LayerBitmap* layer = nullptr;

        DeformHarness()
        {
            Object* object = new Object;
            object->init();

            editor = new Editor;
            scribbleArea = new TestScribbleArea(nullptr);
            editor->setScribbleArea(scribbleArea);
            editor->setObject(object);
            scribbleArea->setEditor(editor);
            editor->init();
            scribbleArea->init();

            // the layer must exist before the tool switch: the previous
            // (stroke) tool flushes its buffer on leavingThisTool
            layer = editor->layers()->createBitmapLayer("bench");
            editor->layers()->setCurrentLayer(0);
            editor->scrubTo(1);

            editor->tools()->setCurrentTool(DEFORM);

            BitmapImage* img = static_cast<BitmapImage*>(layer->getKeyFrameAt(1));
            QImage blank(200, 200, QImage::Format_ARGB32_Premultiplied);
            blank.fill(QColor(255, 0, 0, 255));
            // a vertical blue band so displacement is observable
            for (int y = 0; y < 200; y++) {
                QRgb* line = reinterpret_cast<QRgb*>(blank.scanLine(y));
                for (int x = 95; x < 105; x++) {
                    line[x] = qPremultiply(qRgba(0, 0, 255, 255));
                }
            }
            // paste() keeps the frame bounds in sync (a raw image swap would
            // leave bounds empty and the deform session would see no content)
            BitmapImage content(QPoint(0, 0), blank);
            img->paste(&content);
        }

        ~DeformHarness()
        {
            delete editor;
            delete scribbleArea;
        }

        DeformTool* deformTool() const
        {
            return dynamic_cast<DeformTool*>(editor->tools()->getTool(DEFORM));
        }

        void press(const QPointF& pos)
        {
            static_cast<TestScribbleArea*>(scribbleArea)->testMousePress(pos);
        }

        void move(const QPointF& pos)
        {
            static_cast<TestScribbleArea*>(scribbleArea)->testMouseMove(pos);
        }

        void release(const QPointF& pos)
        {
            static_cast<TestScribbleArea*>(scribbleArea)->testMouseRelease(pos);
        }
    };
}

TEST_CASE("DeformTool liquify drag moves pixels")
{
    DeformHarness h;

    DeformTool* tool = h.deformTool();
    REQUIRE(tool != nullptr);
    tool->setDeformMode(0); // liquify

    BitmapImage* img = static_cast<BitmapImage*>(h.layer->getKeyFrameAt(1));
    const QRgb before = img->image()->pixel(100, 100);

    h.press(QPointF(80, 100));
    REQUIRE(tool->isActive()); // entering the tool arms the session
    for (int i = 1; i <= 20; i++)
    {
        h.move(QPointF(80 + i * 3, 100));
    }
    h.release(QPointF(140, 100));

    tool->leavingThisTool(); // commit

    const QRgb after = img->image()->pixel(100, 100);
    INFO("before=" << qRed(before) << "," << qGreen(before) << "," << qBlue(before)
         << " after=" << qRed(after) << "," << qGreen(after) << "," << qBlue(after));
    REQUIRE(before != after);
}

TEST_CASE("DeformTool no-op click keeps image")
{
    DeformHarness h;

    DeformTool* tool = h.deformTool();
    tool->setDeformMode(0);

    BitmapImage* img = static_cast<BitmapImage*>(h.layer->getKeyFrameAt(1));
    const QRgb before = img->image()->pixel(100, 100);

    h.press(QPointF(100, 100));
    h.release(QPointF(100, 100));

    tool->leavingThisTool();

    REQUIRE(img->image()->pixel(100, 100) == before);
}
