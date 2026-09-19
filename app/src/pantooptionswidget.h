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
#ifndef PANTOOPTIONSWIDGET_H
#define PANTOOPTIONSWIDGET_H

#include "basewidget.h"

#include <QPointF>

class QComboBox;
class Editor;
class PantoTool;
class SpinSlider;

/** 仿制图章（Panto）工具选项面板：取样源 / 偏移模式 / 偏移量 / 复位。
 *  笔尖大小/压感等通用参数由描边选项面板（StrokeOptionsWidget）负责 */
class PantoOptionsWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit PantoOptionsWidget(Editor* editor, QWidget* parent = nullptr);

    void initUI() override;
    void updateUI() override;

private:
    void applyFromWidgets();

    Editor* mEditor = nullptr;
    PantoTool* mTool = nullptr;

    QComboBox* mSourceCombo = nullptr;
    QComboBox* mOffsetCombo = nullptr;
    SpinSlider* mOffsetXSlider = nullptr;
    SpinSlider* mOffsetYSlider = nullptr;
    bool mSyncing = false;
};

#endif // PANTOOPTIONSWIDGET_H
