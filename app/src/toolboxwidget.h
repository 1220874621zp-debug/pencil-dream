/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang
Copyright (C) 2024-2099 Oliver S. Larsen

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef TOOLBOXWIDGET_H
#define TOOLBOXWIDGET_H

#include <QObject>
#include <QToolButton>
#include <QWidget>

#include "flowlayout.h"
#include "pencildef.h"
#include "toolboxlayout.h"

class Editor;
class QMenu;
class QTimer;

namespace Ui {
class ToolBoxWidget;
}

class ToolBoxWidget : public QWidget
{
    Q_OBJECT
public:
    ToolBoxWidget(QWidget* parent = nullptr);
    ~ToolBoxWidget() override;

    void setEditor(Editor* editor) { mEditor = editor; }
    void initUI();
    void updateUI();

public slots:
    void setActiveTool(ToolType toolType);

public:

    void pencilOn();
    void eraserOn();
    void selectOn();
    void lassoOn();
    void deformOn();
    void moveOn();
    void onionAlignOn();
    void penOn();
    void handOn();
    void polylineOn();
    void bucketOn();
    void eyedropperOn();
    void brushOn();
    void smudgeOn();

    void updateLayoutAlignment();
    void deselectAllTools();
    void toolOn(ToolType toolType, QToolButton* toolButton);

protected:
    int getMinHeightForWidth(int width) const;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    // 工具组按钮（PS 式）：一个按钮承载多个同类工具变体。
    // 短按=激活当前变体；长按=弹出变体菜单；图标右下角小三角标记。
    struct VariantGroup
    {
        QToolButton* button = nullptr;
        QMenu* menu = nullptr;
        QTimer* holdTimer = nullptr;
        ToolType current = INVALID_TOOL;
    };

    void setupVariantGroup(VariantGroup& group, QToolButton* button,
                           ToolType defaultVariant, const QList<ToolType>& variants);
    void setVariant(VariantGroup& group, ToolType toolType);
    void variantOn(VariantGroup& group);
    void showVariantMenu(VariantGroup& group);

    static QString variantName(ToolType toolType);
    static QString variantDesc(ToolType toolType);
    static QString variantCommand(ToolType toolType);

    FlowLayout* mFlowlayout = nullptr;
    VariantGroup mSelectionGroup;   // 套索 + 矩形选择
    VariantGroup mTransformGroup;   // 变形 + 移动

    Ui::ToolBoxWidget* ui = nullptr;
    Editor* mEditor = nullptr;
};

#endif // TOOLBOXWIDGET_H
