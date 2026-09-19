/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang
Copyright (C) 2026 Pencil Dream contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef PANTOTOOL_H
#define PANTOTOOL_H

#include "stroketool.h"
#include "brush/brushengine.h"

#include <QCursor>
#include <QImage>
#include <QPointF>
#include <QTimer>

class BitmapImage;
class LayerBitmap;

/**
 * Panto 重绘工具：TVP Animation 仿制图章的动画特化版。
 *
 * 取样源可以是上一帧/下一帧（与灯桌完全同源：循环层先 displayFrameFor 回绕，
 * 再按 ONION_TYPE 决定"上一帧"=逐帧±1 的覆盖画面或上一个关键帧）、当前帧
 * （自克隆，读下笔前快照不反馈糊）或帧剪贴板（Spare 临时源）。
 * 起笔时深拷贝源图快照，经 BrushEngine 的 Clone 颜色源按
 * "画布坐标 − 偏移 − 源图原点"采样盖章——笔尖/压感/不透明度/纹理/
 * 同笔不加深全部继承当前笔刷预设（BrushPresetPanel 联动）。
 *
 * 偏移三模式：原位（offset=0，把上一帧原地拓印过来——默认）、固定偏移
 * （Alt+拖动定义向量，跨笔划持久）、跟随（PS 式：Alt+点击定源点，每笔以
 * 落点对齐）。落帧/撤销走 BrushTool 同款管线（起笔 createState 快照，
 * 松手 record → BitmapReplaceCommand 单步撤销）。
 */
class PantoTool : public StrokeTool
{
    Q_OBJECT

public:
    explicit PantoTool(QObject* parent);

    ToolType type() const override;

    ToolProperties& toolProperties() override { return mSettings.toolProperties(); }
    const StrokeToolProperties& strokeToolProperties() const override { return mSettings; }

    void loadSettings() override;
    QCursor cursor() override;

    void pointerMoveEvent(PointerEvent*) override;
    void pointerPressEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;

    bool keyPressEvent(QKeyEvent* event) override;
    bool keyReleaseEvent(QKeyEvent* event) override;

    // ---- 取样源 / 偏移模式（选项面板数据口） ----
    enum class SourceMode
    {
        PreviousFrame = 0,
        NextFrame,
        CurrentFrame,
        Clipboard
    };
    enum class OffsetMode
    {
        InPlace = 0,     // 原位拓印：offset 恒 0
        FixedOffset,     // 固定偏移：Alt+拖定义，跨笔划持久
        Follow           // 跟随（PS 式）：Alt+点击定源点，每笔以首 dab 对齐
    };

    SourceMode sourceMode() const { return mSourceMode; }
    void setSourceMode(SourceMode mode);
    OffsetMode offsetMode() const { return mOffsetMode; }
    void setOffsetMode(OffsetMode mode);
    QPointF cloneOffset() const { return mOffset; }
    void setCloneOffset(const QPointF& offset);
    void resetOffset();

    // ---- 笔刷预设联动（BrushPresetPanel / 启动恢复） ----
    void applyBrushPreset(const BrushSettings& preset);
    void initPresetExtras(const BrushSettings& preset);
    BrushSettings currentBrushSettings();

signals:
    void pantoOptionsChanged();

protected:
    int emptyFrameActionOverride() const override { return CREATE_NEW_KEY; }

private:
    void drawStroke();
    void paintAt(QPointF point);
    /** 解析取样源→深拷贝快照→按当前偏移模式注入引擎；false=无可用源 */
    bool prepareCloneSource();
    /** 上一/下一取样帧的图像（灯桌同源步进），找不到返回 nullptr */
    BitmapImage* ghostSourceImage() const;
    void syncEngineSettings();
    void persistUserOptions();
    void persistPantoOptions();
    BrushEngine::DabPainter dabPainter() const;

    StrokeToolProperties mSettings;
    // 预设里超出工具属性范围的参数（笔尖形状/扁率/角度/间距/曲线等）
    BrushSettings mPresetExtras;
    BrushEngine mEngine;
    QTimer mAirbrushTimer;

    SourceMode mSourceMode = SourceMode::PreviousFrame;
    OffsetMode mOffsetMode = OffsetMode::InPlace;
    QPointF mOffset;                  // 固定偏移（画布坐标，目标 − 源）
    QPointF mMouseDownPoint;

    bool mDefiningOffset = false;     // Alt+拖定义偏移进行中
    QPointF mOffsetDragStart;

    bool mFollowAnchorValid = false;  // 跟随模式源点锚（Alt+点击设定）
    QPointF mFollowAnchor;

    bool mAltHeld = false;

    // 位图光标一次构造终身复用：每次新建 QCursor 都会重建 Windows
    // 原生 HCURSOR，间歇性创建失败会静默回退成系统箭头
    bool mCursorBuilt = false;
    QCursor mCursorSvg;
    QCursor mCursorCross;
};

#endif // PANTOTOOL_H
