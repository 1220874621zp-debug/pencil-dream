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

    editor->color()->setFrontColor(QColor(0, 0, 255));
    editor->tools()->setCurrentTool(BUCKET);
    scribbleArea->testMousePress(QPointF(clickPoint));
    scribbleArea->testMouseRelease(QPointF(clickPoint));

    BitmapImage* after = static_cast<BitmapImage*>(layer->getKeyFrameAt(1));
    if (after)
    {
        const QRect ab = after->bounds();
    }
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
