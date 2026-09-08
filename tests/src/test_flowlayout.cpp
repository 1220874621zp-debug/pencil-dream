/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Pencil2D contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License; version 2.

*/

#include "catch.hpp"

#include "flowlayout.h"

#include <QPushButton>
#include <QWidget>

namespace
{
    // 直接驱动 setGeometry → applyLayout，绕开事件循环，确定性复现
    // 上游 PR#1916 缺陷：无 HCenter/Justify 对齐的 FlowLayout 一旦发生换行，
    // 末行对齐分支会对空 rowAlignments 调用 last()，Release 下读越界内存崩溃
    // （中割对位工具选项面板曾以该形态触发，崩溃在 FlowLayout::applyLayout）
    void exerciseWrappingLayout(bool withAlignment)
    {
        QWidget host;
        FlowLayout* layout = new FlowLayout(&host, 4, 4, 4);
        if (withAlignment)
        {
            layout->setAlignment(Qt::AlignHCenter);
        }

        // 固定尺寸按钮：4×60px + 间距 > 120px 宽度，必然每行至多 2 个
        for (int i = 0; i < 4; i++)
        {
            auto* button = new QPushButton(QString("b%1").arg(i), &host);
            button->setFixedSize(60, 24);
            layout->addWidget(button);
        }

        host.resize(120, 400);
        layout->setGeometry(QRect(0, 0, 120, 400));
    }
}

TEST_CASE("FlowLayout wrapping without alignment must not crash")
{
    exerciseWrappingLayout(false);
    SUCCEED("unaligned wrapping layout survived last-row branch");
}

TEST_CASE("FlowLayout wrapping with HCenter alignment must not crash")
{
    exerciseWrappingLayout(true);
    SUCCEED("centered wrapping layout survived last-row branch");
}
