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
#include "brushpresetpanel.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>

#include "brushtool.h"
#include "erasertool.h"
#include "editor.h"
#include "managers/toolmanager.h"
#include "pencildef.h"

namespace
{

QString presetTooltip(const BrushPreset& preset)
{
    const BrushSettings& s = preset.settings;
    QStringList dyn;
    if (s.pressureSize) dyn << QObject::tr("大小");
    if (s.pressureOpacity) dyn << QObject::tr("不透明度");
    const QString dynamics = dyn.isEmpty() ? QObject::tr("无")
                                           : dyn.join(QObject::tr("、"));
    return QObject::tr("%1｜直径 %2px｜硬度 %3%\n笔尖：%4｜压感控制：%5\n%6")
           .arg(s.eraser ? QObject::tr("橡皮") : QObject::tr("画笔"))
           .arg(qRound(s.diameter))
           .arg(qRound(s.hardness * 100))
           .arg(s.tipShape == BrushSettings::TipShape::Circle ? QObject::tr("圆形") : QObject::tr("方形"))
           .arg(dynamics)
           .arg(preset.builtIn ? QObject::tr("内置笔刷") : QObject::tr("用户笔刷"));
}

} // namespace

BrushPresetPanel::BrushPresetPanel(QWidget* parent)
    : BaseDockWidget(parent)
{
}

BrushPresetPanel::~BrushPresetPanel()
{
}

void BrushPresetPanel::initUI()
{
    setWindowTitle(tr("笔刷"));

    QWidget* content = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto makeButton = [this](const QString& text) {
        QToolButton* button = new QToolButton(this);
        button->setText(text);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setAutoRaise(true);
        return button;
    };

    mCreateButton = makeButton(tr("新建"));
    mDeleteButton = makeButton(tr("删除"));
    mImportButton = makeButton(tr("导入"));
    mExportButton = makeButton(tr("导出"));

    mCreateButton->setToolTip(tr("把当前画笔参数保存为新预设"));
    mDeleteButton->setToolTip(tr("删除选中的用户预设"));
    mImportButton->setToolTip(tr("从 .pbp 文件导入预设"));
    mExportButton->setToolTip(tr("把选中的预设导出为 .pbp 文件"));

    QHBoxLayout* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(4);
    buttonRow->addWidget(mCreateButton);
    buttonRow->addWidget(mDeleteButton);
    buttonRow->addWidget(mImportButton);
    buttonRow->addWidget(mExportButton);
    buttonRow->addStretch();

    mList = new QListWidget(this);
    mList->setViewMode(QListWidget::IconMode);
    mList->setIconSize(QSize(64, 48));
    mList->setGridSize(QSize(76, 82));
    mList->setResizeMode(QListWidget::Adjust);
    mList->setMovement(QListWidget::Static);
    mList->setSpacing(4);
    mList->setWordWrap(true);
    mList->setUniformItemSizes(true);
    mList->setSelectionMode(QAbstractItemView::SingleSelection);
    mList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    layout->addLayout(buttonRow);
    layout->addWidget(mList);
    setWidget(content);

    connect(mCreateButton, &QToolButton::clicked, this, &BrushPresetPanel::onCreatePreset);
    connect(mDeleteButton, &QToolButton::clicked, this, &BrushPresetPanel::onDeletePreset);
    connect(mImportButton, &QToolButton::clicked, this, &BrushPresetPanel::onImportPreset);
    connect(mExportButton, &QToolButton::clicked, this, &BrushPresetPanel::onExportPreset);
    connect(mList, &QListWidget::itemSelectionChanged, this, &BrushPresetPanel::onSelectionChanged);

    mStore.load();
    populateList();

    // 恢复上次选中的笔刷；只装载预设参数，宽度/羽化沿用用户上次的调整
    QSettings settings(PENCIL2D, PENCIL2D);
    QString name = settings.value("Brush/Preset", QStringLiteral("圆头笔")).toString();
    if (mStore.indexOf(name) < 0) {
        name = mStore.presets().isEmpty() ? QString() : mStore.presets().first().name;
    }
    if (!name.isEmpty()) {
        selectPreset(name);
        // 用户调过工具选项（QSettings 有整套存档）则跳过预设回灌，
        // 保持上次会话的完整工作状态；首次启动才应用预设默认值
        const BrushSettings& preset = mStore.presets()[mStore.indexOf(name)].settings;
        if (preset.eraser) {
            EraserTool* tool = dynamic_cast<EraserTool*>(editor()->tools()->getTool(ERASER));
            if (tool && !tool->hasUserOptions()) {
                tool->initPresetExtras(preset);
            }
        } else {
            BrushTool* tool = dynamic_cast<BrushTool*>(editor()->tools()->getTool(BRUSH));
            if (tool && !tool->hasUserOptions()) {
                tool->initPresetExtras(preset);
            }
        }
    }
}

void BrushPresetPanel::updateUI()
{
}

void BrushPresetPanel::populateList()
{
    mPopulating = true;
    mList->clear();

    for (const BrushPreset& preset : mStore.presets()) {
        QPixmap thumbnail = QPixmap::fromImage(preset.thumbnail);
        thumbnail.setDevicePixelRatio(2.0); // 缩略图按 128x96 渲染，显示为 64x48

        QListWidgetItem* item = new QListWidgetItem();
        item->setIcon(QIcon(thumbnail));
        item->setText(preset.name);
        item->setData(Qt::UserRole, preset.name);
        item->setToolTip(presetTooltip(preset));
        item->setTextAlignment(Qt::AlignCenter);
        mList->addItem(item);
    }
    mPopulating = false;
}

void BrushPresetPanel::selectPreset(const QString& name)
{
    mPopulating = true;
    for (int i = 0; i < mList->count(); ++i) {
        if (mList->item(i)->data(Qt::UserRole).toString() == name) {
            mList->setCurrentRow(i);
            break;
        }
    }
    mPopulating = false;
}

void BrushPresetPanel::onSelectionChanged()
{
    if (mPopulating) {
        return;
    }
    QListWidgetItem* item = mList->currentItem();
    if (!item) {
        return;
    }
    const QString name = item->data(Qt::UserRole).toString();
    const int index = mStore.indexOf(name);
    if (index < 0) {
        return;
    }
    // 点预设即切到对应工具：橡皮预设→橡皮，其余→画笔。
    // 程序默认工具是铅笔（不走笔刷引擎），不主动切换的话预设改了也看不出效果
    const ToolType target = mStore.presets()[index].settings.eraser ? ERASER : BRUSH;
    editor()->tools()->setCurrentTool(target);
    if (target == ERASER) {
        if (EraserTool* tool = dynamic_cast<EraserTool*>(editor()->tools()->getTool(ERASER))) {
            tool->applyBrushPreset(mStore.presets()[index].settings);
        }
    } else {
        if (BrushTool* tool = dynamic_cast<BrushTool*>(editor()->tools()->getTool(BRUSH))) {
            tool->applyBrushPreset(mStore.presets()[index].settings);
        }
    }
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue("Brush/Preset", name);
}

void BrushPresetPanel::onCreatePreset()
{
    BaseTool* current = currentPresetTool();
    if (!current) {
        return;
    }
    // 橡皮工具激活时新建的是橡皮预设（eraser 标记随保存进文件）
    const BrushSettings currentSettings = (current->type() == ERASER)
        ? dynamic_cast<EraserTool*>(current)->currentBrushSettings()
        : dynamic_cast<BrushTool*>(current)->currentBrushSettings();
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("新建笔刷预设"),
                                               tr("预设名称:"), QLineEdit::Normal,
                                               tr("我的笔刷"), &ok).trimmed();
    if (!ok || name.isEmpty()) {
        return;
    }

    const int existing = mStore.indexOf(name);
    if (existing >= 0) {
        const bool overWriteBuiltIn = mStore.presets()[existing].builtIn;
        if (overWriteBuiltIn) {
            QMessageBox::warning(this, tr("新建笔刷预设"),
                                  tr("“%1”是内置笔刷的名字，请换一个名字。").arg(name));
            return;
        }
        const auto answer = QMessageBox::question(this, tr("新建笔刷预设"),
                                                  tr("预设“%1”已存在，要覆盖它吗？").arg(name));
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    if (!mStore.saveUserPreset(name, currentSettings)) {
        QMessageBox::warning(this, tr("新建笔刷预设"), tr("预设保存失败。"));
        return;
    }
    populateList();
    selectPreset(name);
}

void BrushPresetPanel::onDeletePreset()
{
    QListWidgetItem* item = mList->currentItem();
    if (!item) {
        return;
    }
    const QString name = item->data(Qt::UserRole).toString();
    const int index = mStore.indexOf(name);
    if (index < 0) {
        return;
    }
    if (mStore.presets()[index].builtIn) {
        QMessageBox::information(this, tr("删除笔刷预设"), tr("内置笔刷不能删除。"));
        return;
    }
    const auto answer = QMessageBox::question(this, tr("删除笔刷预设"),
                                              tr("确定删除预设“%1”吗？").arg(name));
    if (answer != QMessageBox::Yes) {
        return;
    }
    mStore.deleteUserPreset(name);
    populateList();
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.remove("Brush/Preset");
}

void BrushPresetPanel::onImportPreset()
{
    const QString filePath = QFileDialog::getOpenFileName(this, tr("导入笔刷预设"),
                                                          BrushPresetStore::userPresetDir(),
                                                          tr("笔刷预设 (*.pbp)"));
    if (filePath.isEmpty()) {
        return;
    }
    if (!mStore.importPreset(filePath)) {
        QMessageBox::warning(this, tr("导入笔刷预设"),
                             tr("无法读取该文件，可能不是有效的笔刷预设。"));
        return;
    }
    populateList();
    if (!mStore.presets().isEmpty()) {
        selectPreset(mStore.presets().last().name);
    }
}

void BrushPresetPanel::onExportPreset()
{
    QListWidgetItem* item = mList->currentItem();
    if (!item) {
        return;
    }
    const QString name = item->data(Qt::UserRole).toString();
    const QString filePath = QFileDialog::getSaveFileName(this, tr("导出笔刷预设"),
                                                          name + BrushPresetStore::kFileExtension,
                                                          tr("笔刷预设 (*.pbp)"));
    if (filePath.isEmpty()) {
        return;
    }
    if (!mStore.exportPreset(name, filePath)) {
        QMessageBox::warning(this, tr("导出笔刷预设"), tr("预设导出失败。"));
    }
}

BaseTool* BrushPresetPanel::currentPresetTool()
{
    if (!editor()) {
        return nullptr;
    }
    // 预设按当前工具取参数：画笔与橡皮都走笔刷引擎；其余工具退回画笔
    BaseTool* tool = editor()->tools()->currentTool();
    if (tool && (tool->type() == BRUSH || tool->type() == ERASER)) {
        return tool;
    }
    return editor()->tools()->getTool(BRUSH);
}
