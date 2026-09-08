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
class QDoubleSpinBox;
class QSpinBox;
class Editor;
class LayerColorize;

/** 智能填色图层的选项面板：滤波参数（对齐 Krita Colorize Mask），
 *  当前层为填色图层时显示，改参数自动失效全部帧并重算当前帧 */
class ColorizeOptionsWidget : public BaseWidget
{
    Q_OBJECT
public:
    explicit ColorizeOptionsWidget(Editor* editor, QWidget* parent = nullptr);

    void initUI() override;
    void updateUI() override;

private:
    LayerColorize* currentColorizeLayer() const;
    void applyAndRefresh();

    Editor* mEditor = nullptr;

    QCheckBox* mEdgeDetectionCheck = nullptr;
    QDoubleSpinBox* mEdgeSizeSpin = nullptr;
    QDoubleSpinBox* mFuzzyRadiusSpin = nullptr;
    QSpinBox* mCleanUpSpin = nullptr;
};

#endif // COLORIZEOPTIONSWIDGET_H
