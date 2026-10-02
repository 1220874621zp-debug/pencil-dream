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
#ifndef SHADOWOPTIONSWIDGET_H
#define SHADOWOPTIONSWIDGET_H

#include "basewidget.h"
#include "tooloptionwidget.h"

class Editor;
class LayerShadow;
class QLabel;
class QCheckBox;
class QPushButton;
class QToolButton;
class QDoubleSpinBox;

/** 智能阴影图层的选项面板：标记色三槽（可换色/可标透明=禁用线）、
 *  阴影填充色、方向四选一（阴影在左/右/上/下）、闭缝半径、
 *  生成当前帧/全帧生成、警告行、编辑/显示开关。
 *  当前层为智能阴影图层时显示 */
class ShadowOptionsWidget : public BaseWidget
{
    Q_OBJECT
public:
    explicit ShadowOptionsWidget(Editor* editor, QWidget* parent = nullptr);

    void initUI() override;
    void updateUI() override;

private slots:
    void generateCurrentFrame();
    void generateAllFrames();
    void onDirectionClicked(int id);
    void onFillColorClicked();
    void onMarkerColorClicked(int slot);
    void onChangeMarkerColor();
    void onToggleMarkerTransparent(bool checked);
    void onGapRadiusChanged(double value);
    void onEditLinesChanged(bool checked);
    void onShowFillChanged(bool checked);

private:
    LayerShadow* currentShadowLayer() const;
    void refreshSourceLabel();
    void refreshWarnings();
    void refreshMarkerSwatches();
    void invalidateAllFrames(LayerShadow* layer);
    void repaintCanvas();

    Editor* mEditor = nullptr;

    QLabel* mSourceLabel = nullptr;
    QList<QToolButton*> mDirectionButtons;
    QToolButton* mFillColorButton = nullptr;
    QList<QToolButton*> mMarkerButtons;
    QPushButton* mChangeMarkerColorButton = nullptr;
    QPushButton* mTransparentButton = nullptr;
    QDoubleSpinBox* mGapRadiusSpin = nullptr;
    QCheckBox* mEditLinesCheck = nullptr;
    QCheckBox* mShowFillCheck = nullptr;
    QPushButton* mGenerateButton = nullptr;
    QPushButton* mGenerateAllButton = nullptr;
    QLabel* mWarningLabel = nullptr;

    int mSelectedMarkerSlot = 0;
    bool mUpdatingUI = false;
};

#endif // SHADOWOPTIONSWIDGET_H
