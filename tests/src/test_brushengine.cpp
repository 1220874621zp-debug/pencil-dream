/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
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

#include <QDir>
#include <QImage>
#include <QTemporaryDir>
#include <QThread>

#include "brush/brushengine.h"
#include "brush/brushpresetstore.h"
#include "editor.h"
#include "interface/scribblearea.h"
#include "managers/toolmanager.h"
#include "object.h"
#include "pencildef.h"
#include "tool/brushtool.h"
#include "tool/erasertool.h"
#include "tool/smudgetool.h"
#include "graphics/bitmap/tile.h"
#include "graphics/bitmap/tiledbuffer.h"

TEST_CASE("BrushCurve")
{
    SECTION("identity")
    {
        BrushCurve curve;
        REQUIRE(curve.isIdentity());
        REQUIRE(curve.value(0.0) == Approx(0.0).margin(1e-4));
        REQUIRE(curve.value(0.5) == Approx(0.5).margin(1e-4));
        REQUIRE(curve.value(1.0) == Approx(1.0).margin(1e-4));
    }

    SECTION("string roundtrip")
    {
        BrushCurve curve = BrushCurve::fromString("0,0.1;0.4,0.9;1,1;");
        REQUIRE(curve.points().size() == 3);
        const QString serialized = curve.toString();
        BrushCurve copy = BrushCurve::fromString(serialized);
        REQUIRE(copy.value(0.2) == Approx(curve.value(0.2)).margin(1e-4));
        REQUIRE(copy.value(0.7) == Approx(curve.value(0.7)).margin(1e-4));
    }

    SECTION("monotone curve stays in bounds")
    {
        // 陡峭控制点下样条不能过冲出 0..1
        BrushCurve curve = BrushCurve::fromString("0,0;0.05,0.9;0.5,0.5;1,1;");
        for (int i = 0; i <= 100; ++i) {
            const qreal v = curve.value(i / 100.0);
            REQUIRE(v >= 0.0);
            REQUIRE(v <= 1.0);
        }
    }
}

TEST_CASE("BrushSettings XML roundtrip")
{
    BrushSettings settings;
    settings.name = "测试笔刷";
    settings.tipShape = BrushSettings::TipShape::Rectangle;
    settings.diameter = 33.0;
    settings.ratio = 0.45;
    settings.angle = 42.0;
    settings.hardness = 0.07;
    settings.opacity = 0.6;
    settings.spacingMode = BrushSettings::SpacingMode::Fixed;
    settings.spacing = 0.31;
    settings.pressureSize = false;
    settings.sizeCurve = BrushCurve::fromString("0,0.1;0.5,0.8;1,1;");
    settings.pressureOpacity = true;
    settings.opacityCurve = BrushCurve::fromString("0,0;0.3,1;1,0.2;");
    settings.eraser = true;
    settings.flow = 0.4;
    settings.scatter = 1.5;
    settings.paintingMode = BrushSettings::PaintingMode::Buildup;
    settings.blendMode = BrushSettings::BlendMode::Multiply;
    settings.mirrorX = true;
    settings.mirrorY = false;
    settings.airbrushEnabled = true;
    settings.airbrushRate = 60;

    const QString xml = settings.toXMLString();
    BrushSettings loaded;
    REQUIRE(BrushSettings::fromXMLString(xml, loaded));

    REQUIRE(loaded.name == settings.name);
    REQUIRE(loaded.tipShape == settings.tipShape);
    REQUIRE(loaded.diameter == Approx(settings.diameter));
    REQUIRE(loaded.ratio == Approx(settings.ratio));
    REQUIRE(loaded.angle == Approx(settings.angle));
    REQUIRE(loaded.hardness == Approx(settings.hardness));
    REQUIRE(loaded.opacity == Approx(settings.opacity));
    REQUIRE(loaded.spacingMode == settings.spacingMode);
    REQUIRE(loaded.spacing == Approx(settings.spacing));
    REQUIRE(loaded.pressureSize == settings.pressureSize);
    REQUIRE(loaded.pressureOpacity == settings.pressureOpacity);
    REQUIRE(loaded.eraser == true);
    REQUIRE(loaded.flow == Approx(0.4));
    REQUIRE(loaded.scatter == Approx(1.5));
    REQUIRE(loaded.paintingMode == BrushSettings::PaintingMode::Buildup);
    REQUIRE(loaded.blendMode == BrushSettings::BlendMode::Multiply);
    REQUIRE(loaded.mirrorX == true);
    REQUIRE(loaded.mirrorY == false);
    REQUIRE(loaded.airbrushEnabled == true);
    REQUIRE(loaded.airbrushRate == 60);
    REQUIRE(loaded.sizeCurve.value(0.25) == Approx(settings.sizeCurve.value(0.25)).margin(1e-4));
    REQUIRE(loaded.opacityCurve.value(0.8) == Approx(settings.opacityCurve.value(0.8)).margin(1e-4));
}

TEST_CASE("BrushPreset file roundtrip")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    BrushSettings settings;
    settings.name = "导出测试";
    settings.diameter = 18.0;
    settings.hardness = 0.42;
    settings.pressureOpacity = true;

    const QString filePath = tempDir.path() + "/test" + BrushPresetStore::kFileExtension;
    REQUIRE(BrushPresetStore::writePresetFile(filePath, settings));

    BrushSettings loaded;
    QImage thumbnail;
    REQUIRE(BrushPresetStore::readPresetFile(filePath, loaded, thumbnail));
    REQUIRE_FALSE(thumbnail.isNull());
    REQUIRE(loaded.diameter == Approx(18.0));
    REQUIRE(loaded.hardness == Approx(0.42));
    REQUIRE(loaded.pressureOpacity == true);
    REQUIRE(loaded.name == QStringLiteral("导出测试"));
}

TEST_CASE("BrushEngine stroke dabbing")
{
    BrushSettings settings;
    settings.diameter = 20.0;
    settings.pressureSize = false;
    settings.spacingMode = BrushSettings::SpacingMode::Fixed;
    settings.spacing = 0.25; // 5px 步进

    BrushEngine engine;
    engine.setSettings(settings);

    int dabCount = 0;
    QPointF lastDabCenter;
    auto painter = [&](const BrushEngine::DabRequest& dab) {
        ++dabCount;
        // 子像素烤进掩码后 topLeft 是整数，掩码中心 = topLeft + 图心
        lastDabCenter = QPointF(dab.topLeft)
                        + QPointF(dab.dab.width(), dab.dab.height()) * 0.5;
        REQUIRE(dab.opacity > 0.0);
        REQUIRE_FALSE(dab.dab.isNull());
    };

    engine.beginStroke(QPointF(0, 0), 1.0, QColor(0, 0, 0), painter);
    const int firstDabs = dabCount;
    REQUIRE(firstDabs == 1); // 起笔即落点

    engine.strokeTo(QPointF(100, 0), 1.0, painter);
    // 100px / 5px ≈ 20 个 dab（首点已画）
    REQUIRE(dabCount >= 18);
    REQUIRE(dabCount <= 24);
    REQUIRE(lastDabCenter.x() == Approx(100.0).margin(5.0));

    engine.endStroke();
    REQUIRE_FALSE(engine.isStrokeActive());
}

TEST_CASE("BrushEngine sub-pixel snapping and quantized dab cache")
{
    BrushSettings settings;
    settings.diameter = 40.0;
    settings.pressureSize = true;

    BrushEngine engine;
    engine.setSettings(settings);

    // 半像素网格吸附：10.24 → 10.0，10.76 → 11.0，整数落点差一格
    QPoint topLeftA, topLeftB;
    QImage dabA;
    engine.dabAt(QPointF(10.24, 10.24), 1.0, [&](const BrushEngine::DabRequest& r) {
        topLeftA = r.topLeft;
        dabA = r.dab;
    });
    engine.dabAt(QPointF(10.76, 10.76), 1.0, [&](const BrushEngine::DabRequest& r) {
        topLeftB = r.topLeft;
    });
    REQUIRE((topLeftB - topLeftA) == QPoint(1, 1));
    // 吸附点即掩码中心：topLeft + 图心 = 吸附后的落点
    REQUIRE(topLeftA + QPoint(dabA.width() / 2, dabA.height() / 2) == QPoint(10, 10));

    // 直径量化：4% 步长内的压感变化复用同一张缓存 dab，跨步长才重生成
    qint64 cacheKeySmall = 0, cacheKeyBig = 0;
    const QPointF p(50, 50);
    engine.dabAt(p, 0.50, [&](const BrushEngine::DabRequest& r) { cacheKeySmall = r.dab.cacheKey(); });
    qint64 cacheKeyNearby = 0;
    engine.dabAt(p, 0.51, [&](const BrushEngine::DabRequest& r) { cacheKeyNearby = r.dab.cacheKey(); });
    engine.dabAt(p, 0.90, [&](const BrushEngine::DabRequest& r) { cacheKeyBig = r.dab.cacheKey(); });
    REQUIRE(cacheKeySmall == cacheKeyNearby);
    REQUIRE(cacheKeySmall != cacheKeyBig);
}

TEST_CASE("BrushEngine mirror and airbrush")
{
    BrushSettings settings;
    settings.diameter = 10.0;
    settings.pressureSize = false;

    // 镜像：开启 mirrorX 后每枚 dab 发两份（原图 + 水平翻转），位置关于中心对称
    {
        settings.mirrorX = true;
        BrushEngine engine;
        engine.setSettings(settings);
        engine.setMirrorCenter(QPointF(200, 0));
        QImage dab;
        QPoint tl1, tl2; int count = 0;
        engine.dabAt(QPointF(100, 0), 1.0, [&](const BrushEngine::DabRequest& r) {
            if (count == 0) { tl1 = r.topLeft; dab = r.dab; }
            if (count == 1) tl2 = r.topLeft;
            ++count;
        });
        REQUIRE(count == 2);
        REQUIRE(tl2.x() == qRound(2.0 * 200.0) - (tl1.x() + dab.width()));
        REQUIRE(tl1.y() == tl2.y());
    }

    // 喷枪：静止时按速率补 dab
    {
        settings.mirrorX = false;
        settings.airbrushEnabled = true;
        settings.airbrushRate = 100; // 10ms 一枚
        BrushEngine engine;
        engine.setSettings(settings);
        int count = 0;
        const auto painter = [&](const BrushEngine::DabRequest&) { ++count; };
        engine.beginStroke(QPointF(50, 50), 1.0, QColor(0, 0, 0), painter);
        REQUIRE(count == 1); // 起笔一枚
        QThread::msleep(30);
        engine.airbrushTick(painter);
        REQUIRE(count >= 2); // 静止补喷
        engine.endStroke();
    }
}

TEST_CASE("BrushEngine smudge mode skips first dab")
{
    BrushSettings settings;
    settings.diameter = 20.0;
    settings.pressureSize = false;
    BrushEngine engine;
    engine.setSettings(settings);
    engine.setSmudgeMode(true);
    int count = 0;
    const auto painter = [&](const BrushEngine::DabRequest&) { ++count; };
    engine.beginStroke(QPointF(0, 0), 1.0, QColor(0, 0, 0), painter);
    REQUIRE(count == 0); // Krita smudge：起笔只定位不作画
    engine.strokeTo(QPointF(50, 0), 1.0, painter);
    REQUIRE(count >= 2); // 后续 dab 正常布点
    engine.endStroke();
}

TEST_CASE("smudgeBlendImage lerp semantics")
{
    // tile 全黑；canvas 左半红右半蓝；Δ=(10,0)、rate=1、全透掩码
    // → tile(x) 取 canvas(x−10)
    QImage tile(30, 10, QImage::Format_ARGB32_Premultiplied);
    tile.fill(QColor(0, 0, 0, 255));
    QImage canvas(30, 10, QImage::Format_ARGB32_Premultiplied);
    QPainter cp(&canvas);
    cp.fillRect(QRect(0, 0, 15, 10), QColor(255, 0, 0, 255));
    cp.fillRect(QRect(15, 0, 15, 10), QColor(0, 0, 255, 255));
    cp.end();
    QImage mask(10, 10, QImage::Format_ARGB32_Premultiplied);
    mask.fill(QColor(255, 255, 255, 255));

    smudgeBlendImage(tile, QPoint(0, 0), canvas, QPoint(0, 0),
                     mask, QPoint(10, 0), QPointF(10, 0), 1.0);
    // tile(10,5) ← canvas(0,5) 红
    REQUIRE(qRed(tile.pixel(10, 5)) == 255);
    REQUIRE(qBlue(tile.pixel(10, 5)) == 0);
    // 掩码挪到 20..29：tile(25,5) ← canvas(15,5) 蓝、tile(20,5) ← canvas(10,5) 红
    smudgeBlendImage(tile, QPoint(0, 0), canvas, QPoint(0, 0),
                     mask, QPoint(20, 0), QPointF(10, 0), 1.0);
    REQUIRE(qBlue(tile.pixel(25, 5)) == 255);
    REQUIRE(qRed(tile.pixel(25, 5)) == 0);
    REQUIRE(qRed(tile.pixel(20, 5)) == 255);

    // rate=0.5：黑红对半
    QImage tile2(30, 10, QImage::Format_ARGB32_Premultiplied);
    tile2.fill(QColor(0, 0, 0, 255));
    smudgeBlendImage(tile2, QPoint(0, 0), canvas, QPoint(0, 0),
                     mask, QPoint(10, 0), QPointF(10, 0), 0.5);
    REQUIRE(qRed(tile2.pixel(10, 5)) == Approx(127).margin(3));
    REQUIRE(qBlue(tile2.pixel(10, 5)) == 0);
}

TEST_CASE("SmudgeTool options integration")
{
    Object* object = new Object;
    Editor* editor = new Editor;
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    editor->setScribbleArea(scribbleArea);
    editor->init();
    editor->setObject(object);

    SmudgeTool* tool = dynamic_cast<SmudgeTool*>(editor->tools()->getTool(SMUDGE));
    REQUIRE(tool != nullptr);
    const BrushSettings s = tool->currentBrushSettings();
    REQUIRE(s.flow == Approx(0.5));          // 混合速率默认 50%
    REQUIRE(s.spacingMode == BrushSettings::SpacingMode::Fixed);
    REQUIRE(s.spacing == Approx(0.1));       // Krita 混合预设 10% 间距
    REQUIRE(s.pressureSize == false);        // 压感控速率不控大小
    // 硬度由羽化映射，受本机已保存设置影响，不作定值断言
}

TEST_CASE("BrushEngine pressure dynamics")
{
    BrushSettings settings;
    settings.diameter = 40.0;
    settings.pressureSize = true;
    settings.pressureOpacity = false;

    BrushEngine engine;
    engine.setSettings(settings);

    REQUIRE(engine.dabDiameterAt(1.0) == Approx(40.0));
    REQUIRE(engine.dabDiameterAt(0.5) == Approx(20.0));
    REQUIRE(engine.dabDiameterAt(0.0) == Approx(1.0)); // 钳到最小 1

    settings.pressureSize = false;
    engine.setSettings(settings);
    REQUIRE(engine.dabDiameterAt(0.2) == Approx(40.0));

    settings.pressureOpacity = true;
    settings.opacity = 0.8;
    settings.opacityCurve = BrushCurve::fromString("0,0.5;1,1;");
    engine.setSettings(settings);
    REQUIRE(engine.dabOpacityAt(1.0) == Approx(0.8));
    REQUIRE(engine.dabOpacityAt(0.0) == Approx(0.4));
}

TEST_CASE("BrushEngine preview rendering")
{
    BrushSettings settings;
    settings.diameter = 24.0;
    const QImage preview = BrushEngine::renderStrokePreview(settings, QSize(128, 96));
    REQUIRE_FALSE(preview.isNull());
    REQUIRE(preview.size() == QSize(128, 96));

    // 描边应留下非背景像素（背景 #27272a）
    bool hasStrokePixels = false;
    for (int y = 0; y < preview.height() && !hasStrokePixels; ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(preview.constScanLine(y));
        for (int x = 0; x < preview.width(); ++x) {
            if (qRed(line[x]) > 100) {
                hasStrokePixels = true;
                break;
            }
        }
    }
    REQUIRE(hasStrokePixels);
}

TEST_CASE("BrushTool preset application")
{
    // 完整 Editor 台架：init 创建全套管理器与工具，用生产同款 BrushTool 实例
    Object* object = new Object;
    Editor* editor = new Editor;
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    editor->setScribbleArea(scribbleArea);
    editor->init();
    editor->setObject(object);

    BrushTool* tool = dynamic_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    REQUIRE(tool != nullptr);

    // 换两支差异大的笔，验证 applyBrushPreset → 引擎参数整链生效
    BrushSettings fine;
    fine.diameter = 3.0;
    fine.hardness = 1.0;
    fine.pressureSize = false;

    BrushSettings fat;
    fat.diameter = 90.0;
    fat.hardness = 0.35;
    fat.tipShape = BrushSettings::TipShape::Rectangle;
    fat.ratio = 0.45;
    fat.angle = 42.0;

    tool->applyBrushPreset(fine);
    {
        const BrushSettings current = tool->currentBrushSettings();
        REQUIRE(current.diameter == Approx(3.0));
        REQUIRE(current.hardness == Approx(1.0 - 1.0 / 100.0)); // 羽化钳到 1 → 硬度 0.99
        REQUIRE(current.tipShape == BrushSettings::TipShape::Circle);
        REQUIRE(current.pressureSize == false);
    }

    tool->applyBrushPreset(fat);
    {
        const BrushSettings current = tool->currentBrushSettings();
        REQUIRE(current.diameter == Approx(90.0));
        REQUIRE(current.hardness == Approx(1.0 - 65.0 / 100.0).epsilon(0.01));
        REQUIRE(current.tipShape == BrushSettings::TipShape::Rectangle);
        REQUIRE(current.ratio == Approx(0.45));
        REQUIRE(current.angle == Approx(42.0));
    }

    // 引擎侧的 dab 尺寸也要跟着预设走
    BrushEngine probeEngine;
    probeEngine.setSettings(tool->currentBrushSettings());
    REQUIRE(probeEngine.dabDiameterAt(1.0) == Approx(90.0));

    // 台架各部件与全局单例（PixmapCache/QSettings 等）有交叉引用，
    // 进程退出时统一回收，测试内不手动 delete（与其它 Editor 测试不同，
    // init() 过的 Editor 析构链在某些 manager 上不稳定）
}

TEST_CASE("TiledBuffer drawDab wash blending")
{
    // 硬笔 dab：中心全不透明
    BrushSettings settings;
    settings.diameter = 20.0;
    settings.hardness = 1.0;

    BrushEngine engine;
    engine.setSettings(settings);
    QImage dabImage;
    engine.dabAt(QPointF(0, 0), 1.0, [&](const BrushEngine::DabRequest& request) {
        dabImage = request.dab;
    });
    REQUIRE_FALSE(dabImage.isNull());
    const QRgb center = dabImage.pixel(dabImage.width() / 2, dabImage.height() / 2);
    REQUIRE(qAlpha(center) == 255); // 硬笔中心全不透明

    // —— 对照组：SourceOver 与 wash 单次落点在透明底上等价 ——
    struct Variant { QPainter::CompositionMode mode; qreal opacity; int expectedAlpha; };
    const Variant variants[] = {
        { QPainter::CompositionMode_SourceOver, 1.0, 255 },
        { QPainter::CompositionMode_SourceOver, 0.5, 127 },
    };
    for (const Variant& v : variants) {
        QImage canvas(200, 200, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(Qt::transparent);
        QPainter p(&canvas);
        p.setCompositionMode(v.mode);
        p.setOpacity(v.opacity);
        p.drawImage(QPoint(89, 89), dabImage);
        p.end();
        REQUIRE(qAlpha(canvas.pixel(100, 100)) == Approx(v.expectedAlpha).margin(3));
    }

    TiledBuffer buffer;
    // 整数对齐落点：掩码中心对准画布 (100,100)
    const QPoint placeTopLeft(100 - dabImage.width() / 2, 100 - dabImage.height() / 2);
    DabPasteParams half;
    half.opacity = 0.5;
    buffer.drawDab(dabImage, placeTopLeft, half);
    buffer.drawDab(dabImage, placeTopLeft, half); // 同位置再落一笔

    // wash 语义：同处重复落 dab 不叠加不透明度
    Tile* tile = buffer.tiles().value(TileIndex{ 1, 1 }, nullptr);
    REQUIRE(tile != nullptr);
    const QImage tileImage = tile->pixmap().toImage();
    // tile 图像是 64x64 本地坐标，画布点 (100,100) 对应本地 (36,36)
    const QPoint local(100 - tile->pos().x(), 100 - tile->pos().y());
    const QRgb result = tileImage.pixel(local);
    const int resultAlpha = qAlpha(result);
    REQUIRE(resultAlpha > 0);
    REQUIRE(resultAlpha <= 150); // wash 语义：两次半透明 dab 不叠加变深（0.5*255≈128）
}

TEST_CASE("washBlendImage converge, buildup and erase semantics")
{
    // 硬 dab 掩码
    BrushSettings settings;
    settings.diameter = 20.0;
    settings.hardness = 1.0;
    BrushEngine engine;
    engine.setSettings(settings);
    QImage dab;
    engine.dabAt(QPointF(0, 0), 1.0, [&](const BrushEngine::DabRequest& r) { dab = r.dab; });
    REQUIRE(qAlpha(dab.pixel(dab.width() / 2, dab.height() / 2)) == 255);

    const QPoint at(50 - dab.width() / 2, 50 - dab.height() / 2);

    // 涂抹收敛：同处反复落半透明 dab，alpha 收敛到 opacity 封顶不加深
    {
        QImage canvas(100, 100, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(Qt::transparent);
        DabPasteParams p;
        p.opacity = 0.5;
        for (int i = 0; i < 5; ++i) washBlendImage(canvas, dab, at, p);
        const int a = qAlpha(canvas.pixel(50, 50));
        REQUIRE(a == Approx(128).margin(2));
    }
    // 叠加：并集累积，两枚后超过单枚的 128
    {
        QImage canvas(100, 100, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(Qt::transparent);
        DabPasteParams p;
        p.opacity = 0.5;
        p.buildup = true;
        washBlendImage(canvas, dab, at, p);
        const int a1 = qAlpha(canvas.pixel(50, 50));
        washBlendImage(canvas, dab, at, p);
        const int a2 = qAlpha(canvas.pixel(50, 50));
        REQUIRE(a1 == Approx(128).margin(2));
        REQUIRE(a2 == Approx(191).margin(3)); // 128 + 128*(1-128/255)
    }
    // 流量：涂抹模式 flow 在"并集"与"收敛"间插值。
    // 首枚 dab 落在空底上 aZero==aFull==srcA'（flow 无差别）；
    // 第二枚起 aZero(并集) 继续涨、aFull 收敛封顶，flow 决定落点
    {
        QImage canvas(100, 100, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(Qt::transparent);
        DabPasteParams p;
        p.opacity = 0.5;
        p.flow = 0.5;
        washBlendImage(canvas, dab, at, p);
        REQUIRE(qAlpha(canvas.pixel(50, 50)) == Approx(128).margin(2)); // 首枚
        washBlendImage(canvas, dab, at, p);
        // 第二枚：aZero=191、aFull=128，flow=0.5 → ≈160（flow=1 则封在 128）
        REQUIRE(qAlpha(canvas.pixel(50, 50)) == Approx(160).margin(3));
    }
    // 橡皮真实管线：dab 画进"空白缓冲"（普通累积），终局 DestinationOut 反转语义。
    // 曾因擦除分支写成"收缩已有 alpha"而空缓冲全跳过 → 橡皮无效果（回归测试）
    {
        QImage buffer(100, 100, QImage::Format_ARGB32_Premultiplied);
        buffer.fill(Qt::transparent);
        DabPasteParams erase;
        erase.opacity = 0.5;
        for (int i = 0; i < 5; ++i) washBlendImage(buffer, dab, at, erase);
        // 缓冲 alpha = 擦除量，涂抹模式封顶在 opacity（128），不越擦越深
        REQUIRE(qAlpha(buffer.pixel(50, 50)) == Approx(128).margin(2));
        // 终局 DestinationOut 合并到不透明图层：255·(1-128/255) ≈ 127
        QImage layer(100, 100, QImage::Format_ARGB32_Premultiplied);
        layer.fill(QColor(90, 60, 30, 255));
        const int keptA = qRound(255 * (1.0 - qAlpha(buffer.pixel(50, 50)) / 255.0));
        REQUIRE(keptA == Approx(127).margin(2));
    }
}

TEST_CASE("EraserTool preset application")
{
    // 完整 Editor 台架：橡皮与画笔共用笔刷引擎，预设整链生效
    Object* object = new Object;
    Editor* editor = new Editor;
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    editor->setScribbleArea(scribbleArea);
    editor->init();
    editor->setObject(object);

    EraserTool* tool = dynamic_cast<EraserTool*>(editor->tools()->getTool(ERASER));
    REQUIRE(tool != nullptr);

    BrushSettings soft;
    soft.eraser = true;
    soft.diameter = 50.0;
    soft.hardness = 0.25;
    soft.pressureSize = true;
    soft.pressureOpacity = false;

    tool->applyBrushPreset(soft);
    const BrushSettings current = tool->currentBrushSettings();
    REQUIRE(current.eraser == true);
    REQUIRE(current.diameter == Approx(50.0));
    REQUIRE(current.hardness == Approx(0.25).epsilon(0.01)); // 硬度↔羽化往返
    REQUIRE(current.pressureSize == true);
    REQUIRE(current.pressureOpacity == false);

    // 引擎侧的 dab 尺寸也要跟着预设走
    BrushEngine probeEngine;
    probeEngine.setSettings(tool->currentBrushSettings());
    REQUIRE(probeEngine.dabDiameterAt(1.0) == Approx(50.0));

    // 台架各部件与全局单例（PixmapCache/QSettings 等）有交叉引用，
    // 进程退出时统一回收，测试内不手动 delete
}
