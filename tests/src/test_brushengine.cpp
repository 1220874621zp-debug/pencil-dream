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

#include "brush/brushengine.h"
#include "brush/brushpresetstore.h"
#include "editor.h"
#include "interface/scribblearea.h"
#include "managers/toolmanager.h"
#include "object.h"
#include "pencildef.h"
#include "tool/brushtool.h"
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
        lastDabCenter = dab.center;
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

    // —— 对照组：Lighten+opacity 与 SourceOver 在透明底上单次落点等价 ——
    struct Variant { QPainter::CompositionMode mode; qreal opacity; int expectedAlpha; };
    const Variant variants[] = {
        { QPainter::CompositionMode_SourceOver, 1.0, 255 },
        { QPainter::CompositionMode_SourceOver, 0.5, 127 },
        { QPainter::CompositionMode_Lighten, 1.0, 255 },
        { QPainter::CompositionMode_Lighten, 0.5, 127 },
    };
    for (const Variant& v : variants) {
        QImage canvas(200, 200, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(Qt::transparent);
        QPainter p(&canvas);
        p.setCompositionMode(v.mode);
        p.setOpacity(v.opacity);
        p.drawImage(QPointF(89.5, 89.5), dabImage);
        p.end();
        REQUIRE(qAlpha(canvas.pixel(100, 100)) == Approx(v.expectedAlpha).margin(3));
    }

    // —— 手动复刻 drawDab 的 tile 绘制路径，验证 tile 绘制本身可行 ——
    {
        Tile manualTile(QPoint(64, 64), QSize(64, 64));
        QPainter p(&manualTile.pixmap());
        p.translate(-manualTile.pos());
        p.setCompositionMode(QPainter::CompositionMode_Lighten);
        p.setOpacity(0.5);
        p.drawImage(QPointF(89.5, 89.5), dabImage);
        p.end();
        const QImage manualImage = manualTile.pixmap().toImage();
        REQUIRE(qAlpha(manualImage.pixel(36, 36)) == Approx(127).margin(3));
    }

    TiledBuffer buffer;
    const QPointF dabCenter(100.5, 100.5);
    buffer.drawDab(dabImage, dabCenter, 0.5);
    buffer.drawDab(dabImage, dabCenter + QPointF(0.5, 0), 0.5); // 同位置再落一笔

    // wash 语义：同处重复落 dab 不叠加不透明度
    Tile* tile = buffer.tiles().value(TileIndex{ 1, 1 }, nullptr);
    REQUIRE(tile != nullptr);
    const QImage tileImage = tile->pixmap().toImage();
    // tile 图像是 64x64 本地坐标，画布点 (100,100) 对应本地 (36,36)
    const QPoint local(100 - tile->pos().x(), 100 - tile->pos().y());
    const QRgb result = tileImage.pixel(local);
    const int resultAlpha = qAlpha(result);
    REQUIRE(resultAlpha > 0);
    REQUIRE(resultAlpha <= 150); // wash 语义：两次半透明 dab 不叠加变深（0.5*255≈127）
}
