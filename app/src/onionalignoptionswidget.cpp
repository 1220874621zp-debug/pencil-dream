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

#include "onionalignoptionswidget.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QToolTip>

#include "editor.h"
#include "toolmanager.h"
#include "onionaligntool.h"

OnionAlignOptionsWidget::OnionAlignOptionsWidget(Editor* editor, QWidget* parent)
    : BaseWidget(parent)
    , mEditor(editor)
{
    initUI();
}

void OnionAlignOptionsWidget::initUI()
{
    mTool = static_cast<OnionAlignTool*>(mEditor->tools()->getTool(ONION_ALIGN));
    Q_ASSERT(mTool != nullptr);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto addToolButton = [this, layout](const QString& text, const QString& tip, auto slot)
    {
        auto* button = new QPushButton(text, this);
        button->setToolTip(tip);
        connect(button, &QPushButton::clicked, mTool, slot);
        layout->addWidget(button);
        return button;
    };

    addToolButton(tr("中心对齐"), tr("前后帧内容中心对齐到中点（等同双击画布）"), &OnionAlignTool::autoAlignCenters);
    addToolButton(tr("复位前帧"), tr("归零红色（前帧）幽灵的偏移"), &OnionAlignTool::resetPrevGhostOffset);
    addToolButton(tr("复位后帧"), tr("归零蓝色（后帧）幽灵的偏移"), &OnionAlignTool::resetNextGhostOffset);
    addToolButton(tr("全部复位"), tr("清空当前图层全部幽灵偏移（等同 Alt+点空白）"), &OnionAlignTool::resetAllGhostOffsets);

    layout->addStretch();
}

void OnionAlignOptionsWidget::updateUI()
{
    // 无状态按钮面板，无需刷新
}
