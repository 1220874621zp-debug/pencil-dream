/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Pencil2D contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef ONIONALIGNOPTIONSWIDGET_H
#define ONIONALIGNOPTIONSWIDGET_H

#include "basewidget.h"

class Editor;
class OnionAlignTool;

/** 洋葱皮对位工具的选项面板：复位/自动对齐入口（拖拽与 Alt+点击之外的显式按钮） */
class OnionAlignOptionsWidget : public BaseWidget
{
    Q_OBJECT
public:
    explicit OnionAlignOptionsWidget(Editor* editor, QWidget* parent = nullptr);

    void initUI() override;
    void updateUI() override;

private:
    Editor* mEditor = nullptr;
    OnionAlignTool* mTool = nullptr;
};

#endif // ONIONALIGNOPTIONSWIDGET_H
