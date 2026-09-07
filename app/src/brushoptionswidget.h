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
#ifndef BRUSHOPTIONSWIDGET_H
#define BRUSHOPTIONSWIDGET_H

#include "basewidget.h"

#include <QComboBox>
#include <QGroupBox>
#include <QWidget>

#include "brush/brushsettings.h"

class BrushTool;
class Editor;
class QCheckBox;
class SpinSlider;

/**
 * 画笔/橡皮工具选项面板（全面参考 Krita 像素笔刷的工具选项）：
 * 笔尖（大小/不透明度/流量/硬度/纵横比/角度）+ 描边（间距）+ 动态（压感/防抖）
 * + 高级（散布/喷枪/涂料模式/笔尖混合/镜像）。数据走 BrushSettings 整套存取。
 */
class BrushOptionsWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit BrushOptionsWidget(Editor* editor, QWidget* parent = nullptr);
    ~BrushOptionsWidget() override;

    void initUI() override;
    void updateUI() override;

private:
    class BaseTool* currentPresetCapableTool() const;
    void applyFromWidgets();

    Editor* mEditor = nullptr;

    // 笔尖
    SpinSlider* mSizeSlider = nullptr;
    SpinSlider* mOpacitySlider = nullptr;
    SpinSlider* mFlowSlider = nullptr;
    SpinSlider* mHardnessSlider = nullptr;
    SpinSlider* mRatioSlider = nullptr;
    SpinSlider* mAngleSlider = nullptr;

    // 描边
    QCheckBox* mAutoSpacingBox = nullptr;
    SpinSlider* mSpacingCoeffSlider = nullptr;
    SpinSlider* mSpacingSlider = nullptr;

    // 动态
    QCheckBox* mPressureBox = nullptr;
    QComboBox* mStabilizerCombo = nullptr;

    // 高级
    SpinSlider* mScatterSlider = nullptr;
    QCheckBox* mAirbrushBox = nullptr;
    SpinSlider* mAirbrushRateSlider = nullptr;
    QComboBox* mPaintingModeCombo = nullptr;
    QWidget* mPaintingModeRow = nullptr;
    QComboBox* mBlendModeCombo = nullptr;
    QGroupBox* mBlendModeGroup = nullptr;
    QWidget* mMirrorRow = nullptr;
    QCheckBox* mMirrorXBox = nullptr;
    QCheckBox* mMirrorYBox = nullptr;

    bool mApplying = false;
};

#endif // BRUSHOPTIONSWIDGET_H
