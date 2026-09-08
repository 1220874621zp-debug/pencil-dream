#include "catch.hpp"

#include "layermanager.h"
#include "filemanager.h"
#include "scribblearea.h"
#include "selectionmanager.h"
#include "toolmanager.h"
#include "layercamera.h"
#include "colormanager.h"

#include <QMouseEvent>

#include "layerbitmap.h"

#include "object.h"
#include "editor.h"
#include "bitmapbucket.h"
#include "bitmapimage.h"

#include "basetool.h"
#include "buckettool.h"

void dragAndFill(QPointF movePoint, Editor* editor, QColor color, QRect bounds, BucketToolProperties properties, int fillCountThreshold) {
    int moveX = 0;

    BitmapBucket bucket = BitmapBucket(editor, color, bounds, movePoint, properties);
    QPointF movingPoint = movePoint;

    int fillCount = 0;
    // stay inside the content: dragging beyond the bounds now also fills the
    // transparent canvas outside (Krita semantics), which would add an extra
    // fill event to the count
    while (moveX < bounds.width() - 5) {
        moveX++;
        movingPoint.setX(movingPoint.x()+1);

        bucket.paint(movingPoint, [&fillCount] (BucketState state, int, int ) {

            if (state == BucketState::DidFillTarget) {
                fillCount++;
            }
        });
    }

    REQUIRE(fillCount == fillCountThreshold);
}

void verifyOnlyPixelsInsideSegmentsAreFilled(QPoint referencePoint, const BitmapImage* image, QRgb fillColor)
{
    REQUIRE(image->constScanLine(referencePoint.x(), referencePoint.y()) == fillColor);

    // pixels that are not transparent nor the given fill color, should be left untouched
    REQUIRE(image->constScanLine(referencePoint.x()+4, referencePoint.y()) != fillColor);
    REQUIRE(image->constScanLine(referencePoint.x()+5, referencePoint.y()) != fillColor);
    REQUIRE(image->constScanLine(referencePoint.x()+6, referencePoint.y()) != fillColor);
}

/**
 *    Ascii representation of test project
 *    The "*" represent black strokes.
 *    The space inbetween represents transparency
 *    ***************
 *    *  *   *   *  *
 *    ***************
 *
 *    The test cases are based around filling on the initially transparent area and dragging across the four segments.
 */
TEST_CASE("BitmapBucket - Fill drag behaviour across four segments")
{
    FileManager fm;
    Object* obj = fm.load(":/fill-drag-test/fill-drag-test.pcl");
    Editor* editor = new Editor;
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    editor->setScribbleArea(scribbleArea);
    editor->setObject(obj);
    editor->init();

    BucketToolProperties properties;
    QSettings settings;

    QHash<int, PropertyInfo> info;

    info[BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE] = 0;
    info[BucketToolProperties::FILLEXPAND_ENABLED] = false;
    info[BucketToolProperties::FILLMODE_VALUE] = 0;
    info[BucketToolProperties::COLORTOLERANCE_VALUE] = 25;
    info[BucketToolProperties::COLORTOLERANCE_ENABLED] = true;
    properties.toolProperties().insertProperties(info);
    properties.toolProperties().loadFrom("BucketTest", settings);

    QColor fillColor = QColor(255,255,0,100);

    BitmapImage beforeFill = *static_cast<LayerBitmap*>(editor->layers()->currentLayer())->getBitmapImageAtFrame(1);

    QPoint pressPoint = beforeFill.bounds().topLeft();

    pressPoint.setX(pressPoint.x()+3);
    pressPoint.setY(pressPoint.y()+7);

    REQUIRE(beforeFill.constScanLine(pressPoint.x(), pressPoint.y()) == 0);

    // The dragging logic is based around that we only fill on either transparent or the same color as the fill color.
    SECTION("Filling on current layer - layer is not pre filled") {
        Layer* strokeLayer = editor->layers()->currentLayer();
        SECTION("When reference is current layer, only transparent color is filled")
        {
            properties.toolProperties().setBaseValue(BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE, 0);
            dragAndFill(pressPoint, editor, fillColor, beforeFill.bounds(), properties, 4);

            BitmapImage* image = static_cast<LayerBitmap*>(strokeLayer)->getLastBitmapImageAtFrame(1);

            verifyOnlyPixelsInsideSegmentsAreFilled(pressPoint, image, qPremultiply(fillColor.rgba()));
        }

        SECTION("When reference is all layers, only transparent color is filled")
        {
            properties.toolProperties().setBaseValue(BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE, 1);

            dragAndFill(pressPoint, editor, fillColor, beforeFill.bounds(), properties, 4);

            BitmapImage* image = static_cast<LayerBitmap*>(strokeLayer)->getLastBitmapImageAtFrame(1);

            verifyOnlyPixelsInsideSegmentsAreFilled(pressPoint, image, qPremultiply(fillColor.rgba()));
        }
    }

    SECTION("Filling on current layer - layer is pre-filled") {

        // Fill mode is set to `replace` because it makes it easier to compare colors...
        properties.toolProperties().setBaseValue(BucketToolProperties::FILLMODE_VALUE, 1);
        Layer* strokeLayer = editor->layers()->currentLayer();
        SECTION("When reference is current layer, only pixels matching the fill color are filled"){
            properties.toolProperties().setBaseValue(BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE, 0);

            dragAndFill(pressPoint, editor, fillColor, beforeFill.bounds(), properties, 4);
            BitmapImage* image = static_cast<LayerBitmap*>(strokeLayer)->getLastBitmapImageAtFrame(1);

            fillColor = QColor(0,255,0,255);
            dragAndFill(pressPoint, editor, fillColor, beforeFill.bounds(), properties, 4);

            image = static_cast<LayerBitmap*>(strokeLayer)->getLastBitmapImageAtFrame(1);

            verifyOnlyPixelsInsideSegmentsAreFilled(pressPoint, image, fillColor.rgba());
        }

        SECTION("When reference is all layers")
        {
            properties.toolProperties().setBaseValue(BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE, 1);

            dragAndFill(pressPoint, editor, fillColor, beforeFill.bounds(), properties, 4);
            BitmapImage* image = static_cast<LayerBitmap*>(strokeLayer)->getLastBitmapImageAtFrame(1);

            fillColor = QColor(0,255,0,255);
            dragAndFill(pressPoint, editor, fillColor, beforeFill.bounds(), properties, 4);

            image = static_cast<LayerBitmap*>(strokeLayer)->getLastBitmapImageAtFrame(1);

            verifyOnlyPixelsInsideSegmentsAreFilled(pressPoint, image, fillColor.rgba());
        }
    }

    delete scribbleArea;
    delete editor;
}

TEST_CASE("BitmapBucket - fill respects the active selection")
{
    FileManager fm;
    Object* obj = fm.load(":/fill-drag-test/fill-drag-test.pcl");
    Editor* editor = new Editor;
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    editor->setScribbleArea(scribbleArea);
    editor->setObject(obj);
    editor->init();

    BucketToolProperties properties;
    QSettings settings;

    QHash<int, PropertyInfo> info;
    info[BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE] = 0;
    info[BucketToolProperties::FILLEXPAND_ENABLED] = false;
    info[BucketToolProperties::FILLMODE_VALUE] = 0;
    info[BucketToolProperties::COLORTOLERANCE_VALUE] = 25;
    info[BucketToolProperties::COLORTOLERANCE_ENABLED] = true;
    properties.toolProperties().insertProperties(info);
    properties.toolProperties().loadFrom("BucketTest", settings);

    const QColor fillColor = QColor(0, 255, 0, 255);

    BitmapImage beforeFill = *static_cast<LayerBitmap*>(editor->layers()->currentLayer())->getBitmapImageAtFrame(1);
    QPoint pressPoint = beforeFill.bounds().topLeft();
    pressPoint.setX(pressPoint.x() + 3);
    pressPoint.setY(pressPoint.y() + 7);
    REQUIRE(beforeFill.constScanLine(pressPoint.x(), pressPoint.y()) == 0);

    // lasso-style polygon selection: a small box around the press point
    // only - the rest of the fillable segment must stay untouched
    QPolygonF selection;
    selection << QPointF(pressPoint.x() - 2, pressPoint.y() - 3)
              << QPointF(pressPoint.x() + 2, pressPoint.y() - 3)
              << QPointF(pressPoint.x() + 2, pressPoint.y() + 3)
              << QPointF(pressPoint.x() - 2, pressPoint.y() + 3);
    editor->select()->setSelection(selection, true);

    BitmapBucket bucket = BitmapBucket(editor, fillColor, beforeFill.bounds(), pressPoint, properties);
    bucket.paint(pressPoint, [](BucketState, int, int) {});

    BitmapImage* image = static_cast<LayerBitmap*>(editor->layers()->currentLayer())->getLastBitmapImageAtFrame(1);

    // inside the selection: filled
    REQUIRE(image->constScanLine(pressPoint.x(), pressPoint.y()) == qPremultiply(fillColor.rgba()));
    // same fillable segment but outside the selection: untouched
    REQUIRE(image->constScanLine(pressPoint.x(), pressPoint.y() - 6) != qPremultiply(fillColor.rgba()));
    REQUIRE(image->constScanLine(pressPoint.x(), pressPoint.y() + 6) != qPremultiply(fillColor.rgba()));

    delete scribbleArea;
    delete editor;
}

TEST_CASE("BitmapBucket - fill a lasso over empty canvas")
{
    FileManager fm;
    Object* obj = fm.load(":/fill-drag-test/fill-drag-test.pcl");
    Editor* editor = new Editor;
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    editor->setScribbleArea(scribbleArea);
    editor->setObject(obj);
    editor->init();

    BucketToolProperties properties;
    QSettings settings;

    QHash<int, PropertyInfo> info;
    info[BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE] = 0;
    info[BucketToolProperties::FILLEXPAND_ENABLED] = false;
    info[BucketToolProperties::FILLMODE_VALUE] = 0;
    info[BucketToolProperties::COLORTOLERANCE_VALUE] = 25;
    info[BucketToolProperties::COLORTOLERANCE_ENABLED] = true;
    properties.toolProperties().insertProperties(info);
    properties.toolProperties().loadFrom("BucketTest", settings);

    const QColor fillColor = QColor(0, 0, 255, 255);

    BitmapImage beforeFill = *static_cast<LayerBitmap*>(editor->layers()->currentLayer())->getBitmapImageAtFrame(1);
    const QRect content = beforeFill.bounds();

    // click in empty canvas far away from any content, inside a lasso box:
    // this used to be clamped to the content corner and filled nothing
    // visible (or the wrong place entirely)
    QPoint clickPoint = content.topLeft() + QPoint(45, 45);
    REQUIRE(!beforeFill.contains(clickPoint));

    QPolygonF selection;
    selection << QPointF(clickPoint.x() - 4, clickPoint.y() - 4)
              << QPointF(clickPoint.x() + 4, clickPoint.y() - 4)
              << QPointF(clickPoint.x() + 4, clickPoint.y() + 4)
              << QPointF(clickPoint.x() - 4, clickPoint.y() + 4);
    editor->select()->setSelection(selection, true);

    // a camera-like max fill region generously covering the empty canvas
    BitmapBucket bucket = BitmapBucket(editor, fillColor, content.adjusted(-500, -500, 500, 500), clickPoint, properties);
    bucket.paint(clickPoint, [](BucketState, int, int) {});

    BitmapImage* image = static_cast<LayerBitmap*>(editor->layers()->currentLayer())->getLastBitmapImageAtFrame(1);

    // the lassoed empty area got filled at the click position
    REQUIRE(image->constScanLine(clickPoint.x(), clickPoint.y()) == qPremultiply(fillColor.rgba()));
    // just outside the lasso the canvas stays empty
    REQUIRE(image->constScanLine(clickPoint.x(), clickPoint.y() - 8) == 0);
    // the drawn content is untouched (the top row of the test image is a stroke)
    REQUIRE(image->constScanLine(content.left(), content.top()) == beforeFill.constScanLine(content.left(), content.top()));

    delete scribbleArea;
    delete editor;
}

namespace
{
    // exposes the protected widget handlers so the test drives the real
    // event path (mMouseInUse tracking included)
    class BucketTestScribbleArea : public ScribbleArea
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
}

namespace
{

    // 每个用例独立搭台：120x120 透明画布上的指定线稿
    struct FillBench
    {
        Editor* editor = nullptr;
        ScribbleArea* scribbleArea = nullptr;
        LayerBitmap* layer = nullptr;

        ~FillBench()
        {
            delete scribbleArea;
            delete editor;
        }
    };

    FillBench makeBench(std::function<void(QPainter&)> drawLineArt)
    {
        FillBench bench;
        Object* object = new Object;
        object->init();
        bench.editor = new Editor;
        bench.scribbleArea = new ScribbleArea(nullptr);
        bench.editor->setScribbleArea(bench.scribbleArea);
        bench.editor->setObject(object);
        bench.scribbleArea->setEditor(bench.editor);
        bench.editor->init();
        bench.scribbleArea->init();

        bench.layer = bench.editor->layers()->createBitmapLayer("fill");
        bench.editor->layers()->setCurrentLayer(0);
        bench.editor->scrubTo(1);

        BitmapImage* img = static_cast<BitmapImage*>(bench.layer->getKeyFrameAt(1));
        REQUIRE(img != nullptr);

        BitmapImage content(QRect(0, 0, 120, 120), Qt::transparent);
        QPainter painter(content.image());
        painter.setPen(QPen(QColor(0, 0, 0, 255), 1));
        drawLineArt(painter);
        painter.end();
        img->paste(&content);
        return bench;
    }

    // 全属性集（Krita 移植新增键缺省时 PropertyInfo 为 INVALID，读出 -1，
    // 这里显式给定避免依赖兜底）
    BucketToolProperties benchProperties(int regionMode, int closeGap, int grow)
    {
        BucketToolProperties properties;
        QHash<int, PropertyInfo> info;
        info[BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE] = 0;
        info[BucketToolProperties::FILLEXPAND_ENABLED] = false;
        info[BucketToolProperties::FILLMODE_VALUE] = 0;
        info[BucketToolProperties::COLORTOLERANCE_VALUE] = 0;
        info[BucketToolProperties::COLORTOLERANCE_ENABLED] = false;
        info[BucketToolProperties::CLOSEGAP_VALUE] = PropertyInfo(0, 32, 0);
        info[BucketToolProperties::FILLEXPAND_VALUE] = PropertyInfo(-40, 40, 0);
        info[BucketToolProperties::FEATHER_VALUE] = PropertyInfo(0, 40, 0);
        info[BucketToolProperties::ANTIALIASING_ENABLED] = false;
        info[BucketToolProperties::GROWSTOPDARKEST_ENABLED] = false;
        info[BucketToolProperties::REGIONMODE_VALUE] = regionMode;
        info[BucketToolProperties::BOUNDARYCOLOR_VALUE] = static_cast<int>(QColor(Qt::black).rgba());
        info[BucketToolProperties::DRAGMODE_VALUE] = 2;
        properties.toolProperties().insertProperties(info);
        properties.toolProperties().setBaseValue(BucketToolProperties::CLOSEGAP_VALUE, PropertyInfo(closeGap));
        if (grow != 0) {
            properties.toolProperties().setBaseValue(BucketToolProperties::FILLEXPAND_ENABLED, PropertyInfo(true));
            properties.toolProperties().setBaseValue(BucketToolProperties::FILLEXPAND_VALUE, PropertyInfo(grow));
        }
        return properties;
    }

    // 画一个带 4px 底边缺口的黑框（框内/框外仅通过缺口连通）
    void drawGappedBox(QPainter& painter)
    {
        painter.drawLine(20, 20, 100, 20);    // top
        painter.drawLine(20, 20, 20, 100);    // left
        painter.drawLine(100, 20, 100, 100);  // right
        painter.drawLine(20, 100, 58, 100);   // bottom left  → 缺口 x=59..62
        painter.drawLine(63, 100, 100, 100);  // bottom right
    }

    QRgb fillOnce(const FillBench& bench, const BucketToolProperties& properties, QPoint click)
    {
        const QColor fillColor(255, 0, 0, 255);
        BitmapBucket bucket(bench.editor, fillColor, QRect(0, 0, 120, 120), click, properties);
        bool didFill = false;
        bucket.paint(click, [&didFill](BucketState state, int, int) {
            if (state == BucketState::DidFillTarget) didFill = true;
        });
        REQUIRE(didFill);
        BitmapImage* img = static_cast<BitmapImage*>(bench.layer->getLastBitmapImageAtFrame(1));
        return img->constScanLine(click.x(), click.y());
    }
}

TEST_CASE("BitmapBucket - Krita gap closing")
{
    SECTION("gap open: the fill spills through the 4px gap")
    {
        FillBench bench = makeBench(drawGappedBox);
        const QRgb red = qPremultiply(QColor(255, 0, 0, 255).rgba());
        const QRgb at = fillOnce(bench, benchProperties(0, 0, 0), QPoint(60, 60));
        REQUIRE(at == red);                       // 框内已填
        BitmapImage* img = static_cast<BitmapImage*>(bench.layer->getLastBitmapImageAtFrame(1));
        REQUIRE(img->constScanLine(5, 5) == red); // 漏到框外
        REQUIRE(img->constScanLine(110, 110) == red);
    }

    SECTION("close gap 8px: the spill is contained")
    {
        FillBench bench = makeBench(drawGappedBox);
        const QRgb red = qPremultiply(QColor(255, 0, 0, 255).rgba());
        const QRgb at = fillOnce(bench, benchProperties(0, 8, 0), QPoint(60, 60));
        REQUIRE(at == red);                        // 框内仍完整填充
        BitmapImage* img = static_cast<BitmapImage*>(bench.layer->getLastBitmapImageAtFrame(1));
        REQUIRE(img->constScanLine(5, 5) == 0);    // 框外被封闭间隙挡住
        REQUIRE(img->constScanLine(110, 110) == 0);
        REQUIRE(img->constScanLine(60, 95) == red); // 缺口内侧贴线处也填满
    }
}

TEST_CASE("BitmapBucket - Krita fill modes")
{
    SECTION("similar regions fill disconnected areas")
    {
        FillBench bench = makeBench([](QPainter& painter) {
            painter.drawRect(20, 20, 30, 30);  // box A
            painter.drawRect(70, 70, 30, 30);  // box B（完全封闭、不连通）
        });
        const QRgb red = qPremultiply(QColor(255, 0, 0, 255).rgba());
        const QRgb at = fillOnce(bench, benchProperties(1, 0, 0), QPoint(35, 35));
        REQUIRE(at == red);
        BitmapImage* img = static_cast<BitmapImage*>(bench.layer->getLastBitmapImageAtFrame(1));
        REQUIRE(img->constScanLine(85, 85) == red); // 不连通的 box B 也被填
        REQUIRE(img->constScanLine(5, 5) == red);   // 背景透明处全填
        REQUIRE(img->constScanLine(20, 35) == qPremultiply(QColor(0, 0, 0, 255).rgba())); // 线稿不动
    }

    SECTION("until boundary color crosses non-boundary strokes")
    {
        FillBench bench = makeBench([](QPainter& painter) {
            painter.drawRect(20, 20, 60, 60);                  // 黑框
            painter.setPen(QPen(QColor(255, 0, 0, 255), 1));
            painter.drawLine(21, 50, 79, 50);                  // 框内一条红线
        });
        const QRgb red = qPremultiply(QColor(255, 0, 0, 255).rgba());
        // 到边界色模式：只有黑色算边界，红线拦不住
        const QRgb at = fillOnce(bench, benchProperties(2, 0, 0), QPoint(50, 30));
        REQUIRE(at == red);
        BitmapImage* img = static_cast<BitmapImage*>(bench.layer->getLastBitmapImageAtFrame(1));
        REQUIRE(img->constScanLine(50, 60) == red);  // 红线之下的下半框被填
        REQUIRE(img->constScanLine(5, 5) == 0);      // 框外不填（黑框是边界）

        // 对照：连续区域模式容差 0 时红线会拦住填充
        FillBench bench2 = makeBench([](QPainter& painter) {
            painter.drawRect(20, 20, 60, 60);
            painter.setPen(QPen(QColor(255, 0, 0, 255), 1));
            painter.drawLine(21, 50, 79, 50);
        });
        fillOnce(bench2, benchProperties(0, 0, 0), QPoint(50, 30));
        BitmapImage* img2 = static_cast<BitmapImage*>(bench2.layer->getLastBitmapImageAtFrame(1));
        REQUIRE(img2->constScanLine(50, 60) == 0);   // 下半框保持透明
    }

    SECTION("negative grow shrinks the fill")
    {
        FillBench bench = makeBench([](QPainter& painter) {
            painter.drawRect(20, 20, 80, 80);
        });
        const QRgb red = qPremultiply(QColor(255, 0, 0, 255).rgba());
        const QRgb at = fillOnce(bench, benchProperties(0, 0, -3), QPoint(60, 60));
        REQUIRE(at == red);
        BitmapImage* img = static_cast<BitmapImage*>(bench.layer->getLastBitmapImageAtFrame(1));
        REQUIRE(img->constScanLine(60, 60) == red);   // 中心填充
        REQUIRE(img->constScanLine(30, 30) == red);   // 距墙 10px
        REQUIRE(img->constScanLine(22, 60) == 0);     // 距左墙 2px 被腐蚀
        REQUIRE(img->constScanLine(60, 98) == 0);     // 距底墙 2px 被腐蚀
    }
}


TEST_CASE("BucketTool - lasso over a never-drawn keyframe")
{
    // regression: a keyframe nobody has drawn into keeps a null image, and
    // BitmapBucket::paint used to bail out silently on !isLoaded() - lasso +
    // bucket on a fresh empty canvas filled nothing at all
    Object* object = new Object;
    object->init();
    Editor* editor = new Editor;
    BucketTestScribbleArea* scribbleArea = new BucketTestScribbleArea(nullptr);
    editor->setScribbleArea(scribbleArea);
    editor->setObject(object);
    scribbleArea->setEditor(editor);
    editor->init();
    scribbleArea->init();

    LayerBitmap* layer = editor->layers()->createBitmapLayer("empty");
    editor->layers()->setCurrentLayer(0);
    editor->scrubTo(1);

    BitmapImage* img = static_cast<BitmapImage*>(layer->getKeyFrameAt(1));
    REQUIRE(img != nullptr);
    REQUIRE(!img->isLoaded()); // the precondition this bug needs

    const QPoint clickPoint(70, 70);
    editor->tools()->setCurrentTool(LASSO);
    const qreal x0 = clickPoint.x() - 12, y0 = clickPoint.y() - 12;
    const qreal x1 = clickPoint.x() + 12, y1 = clickPoint.y() + 12;
    scribbleArea->testMousePress(QPointF(x0, y0));
    for (int i = 0; i <= 6; ++i)
    {
        const qreal t = qreal(i) / 6.0;
        scribbleArea->testMouseMove(QPointF(x0 + t * (x1 - x0), y0));
    }
    for (int i = 0; i <= 6; ++i)
    {
        const qreal t = qreal(i) / 6.0;
        scribbleArea->testMouseMove(QPointF(x1, y0 + t * (y1 - y0)));
    }
    for (int i = 0; i <= 6; ++i)
    {
        const qreal t = qreal(i) / 6.0;
        scribbleArea->testMouseMove(QPointF(x1 - t * (x1 - x0), y1));
    }
    for (int i = 0; i <= 6; ++i)
    {
        const qreal t = qreal(i) / 6.0;
        scribbleArea->testMouseMove(QPointF(x0, y1 - t * (y1 - y0)));
    }
    scribbleArea->testMouseRelease(QPointF(x0, y0));
    REQUIRE(!editor->select()->selectionClipPath().isEmpty());

    // a click outside the selection must be a cheap no-op
    editor->color()->setFrontColor(QColor(0, 0, 255));
    editor->tools()->setCurrentTool(BUCKET);
    const QPoint outside(clickPoint.x() + 60, clickPoint.y() + 60);
    scribbleArea->testMousePress(QPointF(outside));
    scribbleArea->testMouseRelease(QPointF(outside));

    // the fill inside the lasso
    scribbleArea->testMousePress(QPointF(clickPoint));
    scribbleArea->testMouseRelease(QPointF(clickPoint));

    BitmapImage* after = static_cast<BitmapImage*>(layer->getKeyFrameAt(1));
    REQUIRE(after->isLoaded());
    REQUIRE(after->constScanLine(clickPoint.x(), clickPoint.y()) == qPremultiply(QColor(0, 0, 255).rgba()));
    // outside the lasso the fresh canvas stays empty
    REQUIRE(after->constScanLine(clickPoint.x(), clickPoint.y() - 30) == 0);

    delete scribbleArea;
    delete editor;
}

TEST_CASE("BucketTool - lasso over empty canvas via the real event path")
{
    FileManager fm;
    Object* obj = fm.load(":/fill-drag-test/fill-drag-test.pcl");
    Editor* editor = new Editor;
    BucketTestScribbleArea* scribbleArea = new BucketTestScribbleArea(nullptr);
    editor->setScribbleArea(scribbleArea);
    editor->setObject(obj);
    scribbleArea->setEditor(editor);
    editor->init();
    scribbleArea->init();

    BitmapImage beforeFill = *static_cast<LayerBitmap*>(editor->layers()->currentLayer())->getBitmapImageAtFrame(1);
    const QRect content = beforeFill.bounds();
    const QPoint clickPoint = content.topLeft() + QPoint(45, 45);
    REQUIRE(!beforeFill.contains(clickPoint));

    // the app derives the bucket's max fill region from the camera layer;
    // the test project's camera viewRect is degenerate (29x11), which is
    // exactly the setup that used to kill the fill
    LayerCamera* layerCam = editor->layers()->getCameraLayerBelow(editor->currentLayerIndex());
    REQUIRE(layerCam != nullptr);
    REQUIRE(!layerCam->getViewAtFrame(editor->currentFrame())
                 .inverted().mapRect(layerCam->getViewRect()).contains(clickPoint));

    // lasso a box around the click point through real events
    editor->tools()->setCurrentTool(LASSO);
    const qreal x0 = clickPoint.x() - 4, y0 = clickPoint.y() - 4;
    const qreal x1 = clickPoint.x() + 4, y1 = clickPoint.y() + 4;
    scribbleArea->testMousePress(QPointF(x0, y0));
    for (int i = 0; i <= 8; ++i)
    {
        const qreal t = qreal(i) / 8.0;
        scribbleArea->testMouseMove(QPointF(x0 + t * (x1 - x0), y0));
    }
    for (int i = 0; i <= 8; ++i)
    {
        const qreal t = qreal(i) / 8.0;
        scribbleArea->testMouseMove(QPointF(x1, y0 + t * (y1 - y0)));
    }
    for (int i = 0; i <= 8; ++i)
    {
        const qreal t = qreal(i) / 8.0;
        scribbleArea->testMouseMove(QPointF(x1 - t * (x1 - x0), y1));
    }
    for (int i = 0; i <= 8; ++i)
    {
        const qreal t = qreal(i) / 8.0;
        scribbleArea->testMouseMove(QPointF(x0, y1 - t * (y1 - y0)));
    }
    scribbleArea->testMouseRelease(QPointF(x0, y0));

    REQUIRE(!editor->select()->selectionClipPath().isEmpty());

    // bucket click through real events, with the user's REAL persisted
    // settings (registry key 颜料桶): reference = all layers, tolerance
    // disabled, expand 2 enabled, overlay mode. The zh_CN translation makes
    // the app read a different settings key than an untranslated test run,
    // which is why earlier benches silently used different values.
    editor->color()->setFrontColor(QColor(0, 0, 255));
    BucketTool* bucketTool = dynamic_cast<BucketTool*>(editor->tools()->getTool(BUCKET));
    REQUIRE(bucketTool != nullptr);
    bucketTool->setFillReferenceMode(1);
    bucketTool->setColorToleranceEnabled(false);
    bucketTool->setFillExpandEnabled(true);
    bucketTool->setFillExpand(2);
    bucketTool->setFillMode(0);
    editor->tools()->setCurrentTool(BUCKET);
    scribbleArea->testMousePress(QPointF(clickPoint));
    scribbleArea->testMouseRelease(QPointF(clickPoint));

    BitmapImage* image = static_cast<LayerBitmap*>(editor->layers()->currentLayer())->getLastBitmapImageAtFrame(1);
    const QRgb got = image->constScanLine(clickPoint.x(), clickPoint.y());
    REQUIRE(got != 0);

    delete scribbleArea;
    delete editor;
}
