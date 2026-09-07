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
#ifndef BRUSHPRESETPANEL_H
#define BRUSHPRESETPANEL_H

#include "basedockwidget.h"

#include <QListWidgetItem>

#include "brush/brushpresetstore.h"

class QListWidget;
class QToolButton;

/**
 * 笔刷预设面板（Krita 预设选择器的轻量版）：
 * 描边缩略图网格，点击换笔；支持新建/删除/导入/导出预设。
 */
class BrushPresetPanel : public BaseDockWidget
{
    Q_OBJECT

public:
    explicit BrushPresetPanel(QWidget* parent);
    ~BrushPresetPanel() override;

    void initUI() override;
    void updateUI() override;

private:
    void populateList();
    void selectPreset(const QString& name);

    void onSelectionChanged();
    void onCreatePreset();
    void onDeletePreset();
    void onImportPreset();
    void onExportPreset();

    class BaseTool* currentPresetTool();

    BrushPresetStore mStore;
    QListWidget* mList = nullptr;
    QToolButton* mCreateButton = nullptr;
    QToolButton* mDeleteButton = nullptr;
    QToolButton* mImportButton = nullptr;
    QToolButton* mExportButton = nullptr;
    bool mPopulating = false;
};

#endif // BRUSHPRESETPANEL_H
