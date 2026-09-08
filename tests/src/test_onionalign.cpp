/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Pencil2D contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License; version 2.

*/

#include "catch.hpp"

#include "editor.h"
#include "object.h"
#include "layer.h"
#include "layerbitmap.h"
#include "bitmapimage.h"
#include "scribblearea.h"
#include "toolmanager.h"
#include "preferencemanager.h"
#include "layermanager.h"
#include "viewmanager.h"
#include "onionaligntool.h"

#include <QImage>
#include <QPixmap>
#include <QApplication>
#include <QMouseEvent>

namespace
{
    // 走真实事件路径（同 test_deformtool 台架），并支持修饰键注入。
    // 输入坐标一律为画布坐标：画布原点在部件中心（ViewManager mCentre 平移），
    // 事件构造前经 mapCanvasToScreen 换算成部件坐标。
    class TestScribbleAreaO : public ScribbleArea
    {
    public:
        using ScribbleArea::ScribbleArea;

        void press(const QPointF& canvasPos, Qt::KeyboardModifiers mods = Qt::NoModifier)
        {
            const QPointF w = editor()->view()->mapCanvasToScreen(canvasPos);
            QMouseEvent e(QEvent::MouseButtonPress, w, w, Qt::LeftButton, Qt::LeftButton, mods);
            mousePressEvent(&e);
        }

        void move(const QPointF& canvasPos, Qt::KeyboardModifiers mods = Qt::NoModifier)
        {
            const QPointF w = editor()->view()->mapCanvasToScreen(canvasPos);
            QMouseEvent e(QEvent::MouseMove, w, w, Qt::NoButton, Qt::LeftButton, mods);
            mouseMoveEvent(&e);
        }

        void release(const QPointF& canvasPos, Qt::KeyboardModifiers mods = Qt::NoModifier)
        {
            const QPointF w = editor()->view()->mapCanvasToScreen(canvasPos);
            QMouseEvent e(QEvent::MouseButtonRelease, w, w, Qt::LeftButton, Qt::NoButton, mods);
            mouseReleaseEvent(&e);
        }
    };

    struct OnionAlignHarness
    {
        Editor* editor = nullptr;
        TestScribbleAreaO* scribbleArea = nullptr;
        LayerBitmap* layer = nullptr;

        OnionAlignHarness()
        {
            Object* object = new Object;
            object->init();

            editor = new Editor;
            scribbleArea = new TestScribbleAreaO(nullptr);
            editor->setScribbleArea(scribbleArea);
            editor->setObject(object);
            scribbleArea->setEditor(editor);
            editor->init();
            scribbleArea->init();

            layer = editor->layers()->createBitmapLayer("bench");
            editor->layers()->setCurrentLayer(0);
            editor->scrubTo(1);

            // key@2 红块(0,0)-(100,100)、key@4 蓝块(50,50)-(150,150)，帧3为中割工作帧。
            // 帧3须有自己的空关键帧：否则被 key@2 的曝光块覆盖（hold），
            // 绝对模式「当前覆盖关键帧不画幽灵」→ 前幽灵不可见，命中与对齐都无从谈起。
            layer->addKeyFrame(3, new BitmapImage);
            layer->addKeyFrame(2, new BitmapImage);
            BitmapImage* img2 = static_cast<BitmapImage*>(layer->getKeyFrameAt(2));
            QImage red(100, 100, QImage::Format_ARGB32_Premultiplied);
            red.fill(QColor(255, 0, 0, 255));
            BitmapImage content2(QPoint(0, 0), red);
            img2->paste(&content2);

            layer->addKeyFrame(4, new BitmapImage);
            BitmapImage* img4 = static_cast<BitmapImage*>(layer->getKeyFrameAt(4));
            QImage blue(100, 100, QImage::Format_ARGB32_Premultiplied);
            blue.fill(QColor(0, 0, 255, 255));
            BitmapImage content4(QPoint(50, 50), blue);
            img4->paste(&content4);

            editor->scrubTo(3);
            editor->preference()->set(SETTING::PREV_ONION, true);
            editor->preference()->set(SETTING::NEXT_ONION, true);

            editor->tools()->setCurrentTool(ONION_ALIGN);
            scribbleArea->resize(800, 600);
            scribbleArea->grab(); // 首帧全量绘制
        }

        ~OnionAlignHarness()
        {
            delete editor;
            delete scribbleArea;
        }

        OnionAlignTool* tool() const
        {
            return static_cast<OnionAlignTool*>(editor->tools()->getTool(ONION_ALIGN));
        }

        OnionGhostOffset ghost() const
        {
            return scribbleArea->onionGhostOffsets().value(layer->id());
        }

        /** 内容级差异断言用：全画布相异像素计数（禁 QImage!= 尺寸假阴性） */
        static int pixelDiffCount(const QImage& a, const QImage& b)
        {
            if (a.size() != b.size() || a.format() != b.format()) { return -1; }
            int diff = 0;
            for (int y = 0; y < a.height(); y++)
            {
                for (int x = 0; x < a.width(); x++)
                {
                    if (a.pixel(x, y) != b.pixel(x, y)) { diff++; }
                }
            }
            return diff;
        }
    };
}

TEST_CASE("OnionAlign move drag keeps working")
{
    OnionAlignHarness h;
    const QImage before = h.scribbleArea->grab().toImage();

    h.scribbleArea->press(QPointF(10, 10));
    h.scribbleArea->move(QPointF(30, 10));
    h.scribbleArea->release(QPointF(30, 10));

    const OnionGhostOffset g = h.ghost();
    REQUIRE(g.prevFrameNumber == 2);
    REQUIRE(qAbs(g.prev.offset.x() - 20.0) < 0.01);
    REQUIRE(qAbs(g.prev.offset.y()) < 0.01);
    REQUIRE(g.prev.rotation == 0.0);
    REQUIRE(g.prev.scale == 1.0);

    const QImage after = h.scribbleArea->grab().toImage();
    REQUIRE(OnionAlignHarness::pixelDiffCount(before, after) > 200);
}

TEST_CASE("OnionAlign Ctrl drag rotates ghost")
{
    OnionAlignHarness h;
    const QImage before = h.scribbleArea->grab().toImage();

    // 红幽灵锚点=内容包围盒中心：QRect(0,0,100,100).center()=(49,49)（整数中心）。
    // 按下(10,50)方位角≈178.53°，移动(10,10)方位角=-135° → 旋转≈46.47°
    h.scribbleArea->press(QPointF(10, 50), Qt::ControlModifier);
    h.scribbleArea->move(QPointF(10, 10), Qt::ControlModifier);
    h.scribbleArea->release(QPointF(10, 10), Qt::ControlModifier);

    const OnionGhostOffset g = h.ghost();
    REQUIRE(g.prevFrameNumber == 2);
    REQUIRE(qAbs(qAbs(g.prev.rotation) - 46.47) < 0.5);
    REQUIRE(g.prev.scale == 1.0);

    const QImage after = h.scribbleArea->grab().toImage();
    REQUIRE(OnionAlignHarness::pixelDiffCount(before, after) > 200);
}

TEST_CASE("OnionAlign Shift drag scales ghost")
{
    OnionAlignHarness h;
    const QImage before = h.scribbleArea->grab().toImage();

    // 锚点(50,50)；按下(50,10)距离40，移动(50,-30)距离80 → 缩放2倍
    h.scribbleArea->press(QPointF(50, 10), Qt::ShiftModifier);
    h.scribbleArea->move(QPointF(50, -30), Qt::ShiftModifier);
    h.scribbleArea->release(QPointF(50, -30), Qt::ShiftModifier);

    const OnionGhostOffset g = h.ghost();
    REQUIRE(g.prevFrameNumber == 2);
    REQUIRE(qAbs(g.prev.scale - 2.0) < 0.05);
    REQUIRE(g.prev.rotation == 0.0);

    const QImage after = h.scribbleArea->grab().toImage();
    REQUIRE(OnionAlignHarness::pixelDiffCount(before, after) > 500);
}

TEST_CASE("OnionAlign reset clears full transform")
{
    OnionAlignHarness h;

    h.scribbleArea->press(QPointF(10, 50), Qt::ControlModifier);
    h.scribbleArea->move(QPointF(10, 10), Qt::ControlModifier);
    h.scribbleArea->release(QPointF(10, 10), Qt::ControlModifier);
    h.scribbleArea->press(QPointF(50, 10), Qt::ShiftModifier);
    h.scribbleArea->move(QPointF(50, -30), Qt::ShiftModifier);
    h.scribbleArea->release(QPointF(50, -30), Qt::ShiftModifier);
    h.scribbleArea->press(QPointF(10, 10));
    h.scribbleArea->move(QPointF(30, 10));
    h.scribbleArea->release(QPointF(30, 10));

    REQUIRE(h.ghost().prev.rotation != 0.0);
    REQUIRE(h.ghost().prev.scale != 1.0);
    REQUIRE(!h.ghost().prev.offset.isNull());

    h.tool()->resetPrevGhostOffset();

    const OnionGhostOffset g = h.ghost();
    REQUIRE(g.prev.rotation == 0.0);
    REQUIRE(g.prev.scale == 1.0);
    REQUIRE(g.prev.offset.isNull());
}
