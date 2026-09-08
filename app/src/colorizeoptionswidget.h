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
#ifndef COLORIZEOPTIONSWIDGET_H
#define COLORIZEOPTIONSWIDGET_H

#include "basewidget.h"

class QCheckBox;
class QLabel;
class QDoubleSpinBox;
class QHBoxLayout;
class QPushButton;
class QSpinBox;
class QToolButton;
class Editor;
class LayerColorize;

/** 智能填色图层的选项面板（对齐 Krita Colorize Mask 工具选项）：
 *  更新按钮(手动刷新)/编辑模式/显示填色/颜色列表(透明标记+移除)/滤波参数。
 *  当前层为填色图层时显示 */
class ColorizeOptionsWidget : public BaseWidget
{
    Q_OBJECT
public:
    explicit ColorizeOptionsWidget(Editor* editor, QWidget* parent = nullptr);

    void initUI() override;
    void updateUI() override;

private slots:
    void refreshCurrentFrame();
    void refreshAllFrames();
    void applyParams();

private:
    LayerColorize* currentColorizeLayer() const;
    void refreshColors();
    void invalidateAllFrames(LayerColorize* layer);
    void repaintCanvas();

    Editor* mEditor = nullptr;

    QLabel* mSourceLabel = nullptr;
    QPushButton* mRefreshButton = nullptr;
    QPushButton* mRefreshAllButton = nullptr;
    QCheckBox* mEditKeyStrokesCheck = nullptr;
    QCheckBox* mShowColoringCheck = nullptr;
    QHBoxLayout* mColorsRow = nullptr;
    QList<QToolButton*> mColorButtons;
    qint64 mSelectedColor = -1; // QRgb 值或 -1=未选
    QPushButton* mTransparentButton = nullptr;
    QPushButton* mRemoveButton = nullptr;

    QCheckBox* mEdgeDetectionCheck = nullptr;
    QDoubleSpinBox* mEdgeSizeSpin = nullptr;
    QDoubleSpinBox* mFuzzyRadiusSpin = nullptr;
    QSpinBox* mCleanUpSpin = nullptr;
};

#endif // COLORIZEOPTIONSWIDGET_H
