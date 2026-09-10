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
#include "colorpalettewidget.h"
#include "ui_colorpalette.h"

// Standard libraries
#include <algorithm>
#include <cmath>

// Qt
#include <QDebug>
#include <QListWidgetItem>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QMenu>
#include <QtMath>
#include <QScrollBar>
#include <QAbstractItemModel>
#include <QFontMetrics>
#include <QPainter>

// Project
#include "colorref.h"
#include "object.h"
#include "editor.h"
#include "layerbitmap.h"
#include "colormanager.h"

namespace
{
    // 色块名称：左下角直排，浅色块黑字、深色块白字，超出右缘省略
    void drawSwatchName(QPainter& painter, const QSize& swatchSize, const QColor& swatchColor, const QString& name)
    {
        if (name.isEmpty()) { return; }

        QFont nameFont = painter.font();
        nameFont.setPixelSize(11);
        const QFontMetrics metrics(nameFont);
        const QString shown = metrics.elidedText(name, Qt::ElideRight, swatchSize.width() - 8);

        painter.setFont(nameFont);
        painter.setPen(swatchColor.lightnessF() > 0.6 ? QColor(0x2A, 0x2A, 0x2E) : QColor(0xFF, 0xFF, 0xFF));
        painter.drawText(QRect(4, swatchSize.height() - 16, swatchSize.width() - 8, 14),
                         Qt::AlignLeft | Qt::AlignVCenter, shown);
    }
}

ColorPaletteWidget::ColorPaletteWidget(QWidget* parent) :
    BaseDockWidget(parent),
    ui(new Ui::ColorPalette)
{
    ui->setupUi(this);
}

ColorPaletteWidget::~ColorPaletteWidget()
{
    delete ui;
}

void ColorPaletteWidget::initUI()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    int colorGridSize = settings.value("PreferredColorGridSize", 34).toInt();
    mFitSwatches = settings.value("FitSwatchSize", false).toBool();
    if (mFitSwatches)
    {
        fitSwatchSize();
    }

    mIconSize = QSize(colorGridSize, colorGridSize);

    ui->colorListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    // selection feedback is the swatch border itself: no highlight strip
    ui->colorListWidget->setStyleSheet(
        QStringLiteral("QListWidget::item:selected { background: transparent; border: none; }"
                        "QListWidget::item:hover { background: transparent; }"));

    QString sViewMode = settings.value("ColorPaletteViewMode", "ListMode").toString();
    if (sViewMode == "ListMode")
        setListMode();
    else
        setGridMode();

    buttonStylesheet = "::menu-indicator{ image: none; }"
                             "QPushButton { border: 0px; }"
                             "QPushButton:pressed { border: 1px solid #3A3A40; border-radius: 4px; background-color: #28282E; }"
                             "QPushButton:checked { border: 1px solid #3A3A40; border-radius: 4px; background-color: #28282E; }";

    ui->addColorButton->setStyleSheet(buttonStylesheet);
    ui->removeColorButton->setStyleSheet(buttonStylesheet);

    palettePreferences();

    connect(ui->colorListWidget, &QListWidget::itemClicked, this, &ColorPaletteWidget::clickColorListItem);
    connect(ui->colorListWidget->model(), &QAbstractItemModel::rowsMoved, this, &ColorPaletteWidget::onRowsMoved);

    connect(ui->colorListWidget, &QListWidget::itemDoubleClicked, this, &ColorPaletteWidget::changeColorName);
    connect(ui->colorListWidget, &QListWidget::itemChanged, this, &ColorPaletteWidget::onItemChanged);

    connect(ui->addColorButton, &QPushButton::clicked, this, &ColorPaletteWidget::clickAddColorButton);
    connect(ui->removeColorButton, &QPushButton::clicked, this, &ColorPaletteWidget::clickRemoveColorButton);
    connect(ui->colorListWidget, &QListWidget::customContextMenuRequested, this, &ColorPaletteWidget::showContextMenu);

    // 多色卡：下拉切换 + 新建按钮 + 下拉框右键重命名/删除
    connect(ui->paletteComboBox, QOverload<int>::of(&QComboBox::activated),
            this, &ColorPaletteWidget::paletteActivated);
    connect(ui->newPaletteButton, &QPushButton::clicked, this, &ColorPaletteWidget::clickNewPaletteButton);
    ui->paletteComboBox->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->paletteComboBox, &QWidget::customContextMenuRequested,
            this, &ColorPaletteWidget::showPaletteContextMenu);

    connect(editor(), &Editor::objectLoaded, this, &ColorPaletteWidget::updateUI);
}

void ColorPaletteWidget::refreshPaletteCombo()
{
    if (mObject == nullptr) { return; }
    QSignalBlocker b(ui->paletteComboBox);
    ui->paletteComboBox->clear();
    for (int i = 0; i < mObject->paletteCount(); i++)
    {
        ui->paletteComboBox->addItem(mObject->paletteName(i));
    }
    ui->paletteComboBox->setCurrentIndex(mObject->currentPaletteIndex());
}

void ColorPaletteWidget::paletteActivated(int index)
{
    if (mObject == nullptr) { return; }
    mObject->switchToPalette(index);
    refreshPaletteCombo();

    // 换卡后原选中号可能越界，回退到第一格（空卡跳过，setColorNumber 断言 n>=0）
    if (mObject->getColorCount() > 0)
    {
        editor()->color()->setColorNumber(0);
    }
    refreshColorList();
}

void ColorPaletteWidget::clickNewPaletteButton()
{
    if (mObject == nullptr) { return; }

    const QString defaultName = tr("色卡 %1").arg(mObject->paletteCount() + 1);
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("新建色卡"), tr("色卡名称："),
                                         QLineEdit::Normal, defaultName, &ok);
    if (!ok) { return; }
    if (name.isEmpty()) { name = defaultName; }

    mObject->addPalette(name);
    refreshPaletteCombo();

    if (mObject->getColorCount() > 0)
    {
        editor()->color()->setColorNumber(0);
    }
    refreshColorList();
}

void ColorPaletteWidget::showPaletteContextMenu(const QPoint& pos)
{
    if (mObject == nullptr) { return; }
    const int index = ui->paletteComboBox->currentIndex();
    if (index < 0) { return; }

    QMenu menu(this);
    QAction* renameAction = menu.addAction(tr("重命名色卡"));
    QAction* deleteAction = menu.addAction(tr("删除色卡"));
    deleteAction->setEnabled(mObject->paletteCount() > 1);

    QAction* chosen = menu.exec(ui->paletteComboBox->mapToGlobal(pos));
    if (chosen == renameAction)
    {
        bool ok = false;
        QString name = QInputDialog::getText(this, tr("重命名色卡"), tr("色卡名称："),
                                             QLineEdit::Normal, mObject->paletteName(index), &ok);
        if (ok && !name.isEmpty())
        {
            mObject->renamePalette(index, name);
            refreshPaletteCombo();
        }
    }
    else if (chosen == deleteAction)
    {
        const auto button = QMessageBox::question(this, tr("删除色卡"),
                                                  tr("确定删除色卡“%1”吗？其中 %2 个颜色将一并移除。")
                                                      .arg(mObject->paletteName(index))
                                                      .arg(mObject->getColorCount()),
                                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (button == QMessageBox::Yes)
        {
            mObject->removePalette(index);
            refreshPaletteCombo();
            if (mObject->getColorCount() > 0)
            {
                editor()->color()->setColorNumber(0);
            }
            refreshColorList();
        }
    }
}

void ColorPaletteWidget::updateUI()
{
    mObject = mEditor->object();
    refreshPaletteCombo();
    refreshColorList();
    updateGridUI();
}

void ColorPaletteWidget::setCore(Editor* editor)
{
    mEditor = editor;
    mObject = mEditor->object();
}

void ColorPaletteWidget::showContextMenu(const QPoint& pos)
{
    QPoint globalPos = ui->colorListWidget->mapToGlobal(pos);

    QMenu* menu = new QMenu;
    connect(menu, &QMenu::triggered, menu, &QMenu::deleteLater);

    // 右键落在色块上才提供重命名，并把选中切到该块
    QListWidgetItem* itemUnderCursor = ui->colorListWidget->itemAt(pos);
    if (itemUnderCursor != nullptr)
    {
        ui->colorListWidget->setCurrentItem(itemUnderCursor);
        menu->addAction(tr("重命名"), this, &ColorPaletteWidget::renameItem);
        menu->addSeparator();
    }

    menu->addAction(tr("Add"), this, &ColorPaletteWidget::addItem, 0);
    menu->addAction(tr("Replace"),  this, &ColorPaletteWidget::replaceItem, 0);
    menu->addAction(tr("Remove"), this, &ColorPaletteWidget::removeItem, 0);

    if (mObject != nullptr && mObject->getColorCount() > 1)
    {
        menu->addSeparator();
        menu->addAction(tr("按色相排序"), this, &ColorPaletteWidget::sortPaletteByHue);
    }

    menu->exec(globalPos);
}

void ColorPaletteWidget::sortPaletteByHue()
{
    if (mObject == nullptr) { return; }

    const int count = mObject->getColorCount();
    QVector<ColorRef> refs;
    refs.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        refs.append(mObject->getColor(i));
    }

    // 彩色按色相升序（红→黄→绿→青→蓝→洋红），近无彩色排尾部按明度降序
    std::sort(refs.begin(), refs.end(), [](const ColorRef& a, const ColorRef& b)
    {
        const bool grayA = a.color.saturation() < 10;
        const bool grayB = b.color.saturation() < 10;
        if (grayA != grayB) { return grayB; }
        if (grayA) { return a.color.lightness() > b.color.lightness(); }
        if (a.color.hsvHue() != b.color.hsvHue()) { return a.color.hsvHue() < b.color.hsvHue(); }
        return a.color.saturation() > b.color.saturation();
    });

    for (int i = 0; i < count; ++i)
    {
        mObject->setColorRef(i, refs[i]);
    }
    refreshColorList();
}

void ColorPaletteWidget::renameItem()
{
    QListWidgetItem* item = ui->colorListWidget->currentItem();
    if (item == nullptr) { return; }

    if (ui->colorListWidget->viewMode() == QListView::IconMode)
    {
        changeColorName(item);
    }
    else
    {
        ui->colorListWidget->editItem(item);
    }
}

void ColorPaletteWidget::addItem()
{
    QSignalBlocker b(ui->colorListWidget);
    QColor newColor = mEditor->color()->frontColor(false);

    ColorRef ref(newColor);

    const int colorIndex = ui->colorListWidget->count();
    mObject->addColorAtIndex(colorIndex, ref);

    refreshColorList();

    if (mFitSwatches)
    {
        fitSwatchSize();
    }

    QListWidgetItem* item = ui->colorListWidget->item(colorIndex);
    ui->colorListWidget->editItem(item);
    ui->colorListWidget->scrollToItem(item);
}

void ColorPaletteWidget::replaceItem()
{
    QSignalBlocker b(ui->colorListWidget);
    int index = ui->colorListWidget->currentRow();

    QColor newColor = mEditor->color()->frontColor(false);

    if (index < 0 ) { return; }

    updateItemColor(index, newColor);
    emit colorNumberChanged(index);
    ui->colorListWidget->setCurrentRow(index);
}

void ColorPaletteWidget::removeItem()
{
    QSignalBlocker b(ui->colorListWidget);
    clickRemoveColorButton();
}

void ColorPaletteWidget::selectColorNumber(int colorNumber) const
{
    ui->colorListWidget->setCurrentRow(colorNumber);
}

int ColorPaletteWidget::currentColorNumber()
{
    if (ui->colorListWidget->currentRow() < 0)
    {
        ui->colorListWidget->setCurrentRow(0);
    }
    return ui->colorListWidget->currentRow();
}

void ColorPaletteWidget::refreshColorList()
{
    QSignalBlocker b(ui->colorListWidget);
    if (ui->colorListWidget->count() > 0)
    {
        ui->colorListWidget->clear();
    }

    int colorCount = editor()->object()->getColorCount();

    for (int i = 0; i < colorCount; i++)
    {
        addSwatch(i);
    }

    selectColorNumber(editor()->color()->frontColorNumber());
    updateGridUI();
    update();
}

void ColorPaletteWidget::addSwatch(int colorIndex) const
{
    const QSize tile = swatchTileSize();

    QPixmap originalColorSwatch(tile);
    QPainter painter(&originalColorSwatch);
    painter.drawTiledPixmap(0, 0, tile.width(), tile.height(), QPixmap(":/background/checkerboard.png"));
    painter.end();

    const ColorRef colorRef = mObject->getColor(colorIndex);
    QListWidgetItem* colorItem = new QListWidgetItem(ui->colorListWidget);

    if (ui->colorListWidget->viewMode() != QListView::IconMode)
    {
        colorItem->setText(colorRef.name);
    }
    else
    {
        colorItem->setToolTip(colorRef.name);
    }

    QPixmap colorSwatch = originalColorSwatch;
    QPainter swatchPainter(&colorSwatch);
    swatchPainter.fillRect(0, 0, tile.width(), tile.height(), colorRef.color);

    // 名称画在底图上：普通态/选中态都可见
    if (ui->colorListWidget->viewMode() == QListView::IconMode)
    {
        drawSwatchName(swatchPainter, tile, colorRef.color, colorRef.name);
    }

    QIcon swatchIcon;
    swatchIcon.addPixmap(colorSwatch, QIcon::Normal);

    // Selection = a solid border on the swatch itself: white by default,
    // accent red-pink when the swatch itself is (near) white
    if (ui->colorListWidget->viewMode() == QListView::IconMode)
    {
        QColor selectionBorder = QColor(255, 255, 255);
        if (colorRef.color.lightnessF() > 0.82)
        {
            selectionBorder = QColor(0xE8, 0x38, 0x5A);
        }
        swatchPainter.setPen(QPen(selectionBorder, 2));
        swatchPainter.drawRect(1, 1, tile.width() - 3, tile.height() - 3);
    }
    swatchIcon.addPixmap(colorSwatch, QIcon::Selected);

    colorItem->setIcon(swatchIcon);
    colorItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDragEnabled);

    ui->colorListWidget->addItem(colorItem);
}

void ColorPaletteWidget::changeColorName(QListWidgetItem* item)
{
    Q_ASSERT(item != nullptr);

    if (ui->colorListWidget->viewMode() == QListView::IconMode)
    {
        int colorNumber = ui->colorListWidget->row(item);
        if (colorNumber > -1)
        {
            bool ok;
            QString text = QInputDialog::getText(this,
                                                 tr("Color name"),
                                                 tr("Color name"),
                                                 QLineEdit::Normal,
                                                 mObject->getColor(colorNumber).name,
                                                 &ok);
            if (ok && !text.isEmpty())
            {
                mObject->renameColor(colorNumber, text);
                refreshColorList();
            }
        }
    }
}

void ColorPaletteWidget::onItemChanged(QListWidgetItem* item)
{
    int index = ui->colorListWidget->row(item);
    QString newColorName = item->text();
    mObject->renameColor(index, newColorName);
}

void ColorPaletteWidget::onRowsMoved(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row)
{
    Q_UNUSED(parent)
    Q_UNUSED(destination)
    Q_UNUSED(end)

    int startIndex, endIndex;
    if (start < row)
    {
        row -= 1; // TODO: Is this a bug?
        if (start == row) { return; }

        startIndex = start;
        endIndex = row;

        mObject->movePaletteColor(startIndex, endIndex);
    }
    else
    {
        if (start == row) { return; }

        startIndex = start;
        endIndex = row;

        mObject->movePaletteColor(startIndex, endIndex);
    }

    refreshColorList();
}

void ColorPaletteWidget::clickColorListItem(QListWidgetItem* currentItem)
{
    auto modifiers = qApp->keyboardModifiers();

    // to avoid conflicts with multiple selections
    // ie. will be seen as selected twice and cause problems
    if (modifiers & Qt::ShiftModifier || modifiers & Qt::ControlModifier) { return; }

    int colorIndex = ui->colorListWidget->row(currentItem);

    emit colorNumberChanged(colorIndex);
}

void ColorPaletteWidget::palettePreferences()
{
    ui->colorListWidget->setMinimumWidth(ui->colorListWidget->sizeHintForColumn(0));

    // Let's pretend this button is a separator
    mSeparator = new QAction("", this);
    mSeparator->setSeparator(true);

    buttonStylesheet = "::menu-indicator{ image: none; }"
        "QToolButton { border: 0px; }"
        "QToolButton:pressed { border: 1px solid #3A3A40; border-radius: 4px; background-color: #28282E; }"
        "QToolButton:checked { border: 1px solid #3A3A40; border-radius: 4px; background-color: #28282E; }";


    // Add to UI
    ui->palettePref->setIconSize(QSize(22,22));
    ui->palettePref->setArrowType(Qt::ArrowType::NoArrow);
    ui->palettePref->setStyleSheet(buttonStylesheet);
    ui->palettePref->addAction(ui->listModeAction);
    ui->palettePref->addAction(ui->gridModeAction);

    ui->palettePref->addAction(mSeparator);
    ui->palettePref->addAction(ui->smallSwatchAction);
    ui->palettePref->addAction(ui->mediumSwatchAction);
    ui->palettePref->addAction(ui->largeSwatchAction);
    ui->palettePref->addAction(ui->fitSwatchAction);

    if (mFitSwatches) ui->fitSwatchAction->setChecked(true);
    else if (mIconSize.width() > MEDIUM_ICON_SIZE) ui->largeSwatchAction->setChecked(true);
    else if (mIconSize.width() > MIN_ICON_SIZE) ui->mediumSwatchAction->setChecked(true);
    else ui->smallSwatchAction->setChecked(true);

    if (ui->colorListWidget->viewMode() == QListView::ListMode)
        ui->listModeAction->setChecked(true);
    else
        ui->gridModeAction->setChecked(true);

    connect(ui->listModeAction, &QAction::triggered, this, &ColorPaletteWidget::setListMode);
    connect(ui->gridModeAction, &QAction::triggered, this, &ColorPaletteWidget::setGridMode);
    connect(ui->fitSwatchAction, &QAction::triggered, this, &ColorPaletteWidget::fitSwatchSize);
    connect(ui->smallSwatchAction, &QAction::triggered, this, &ColorPaletteWidget::setSwatchSizeSmall);
    connect(ui->mediumSwatchAction, &QAction::triggered, this, &ColorPaletteWidget::setSwatchSizeMedium);
    connect(ui->largeSwatchAction, &QAction::triggered, this, &ColorPaletteWidget::setSwatchSizeLarge);
}

void ColorPaletteWidget::setListMode()
{
    ui->colorListWidget->setViewMode(QListView::ListMode);
    ui->colorListWidget->setDragDropMode(QAbstractItemView::InternalMove);
    ui->colorListWidget->setGridSize(QSize(-1, -1));
    mStretchedWidth = 0; // 列表模式用基础尺寸
    if (mFitSwatches)
    {
        fitSwatchSize();
    }
    updateUI();

    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue("ColorPaletteViewMode", "ListMode");
}

void ColorPaletteWidget::setGridMode()
{
    ui->colorListWidget->setViewMode(QListView::IconMode);
    ui->colorListWidget->setMovement(QListView::Static); // TODO: update swatch index on move
    ui->colorListWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    mStretchedWidth = 0; // 重新计算列拉伸宽
    updateUI();

    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue("ColorPaletteViewMode", "GridMode");
}

void ColorPaletteWidget::setSwatchSizeSmall()
{
    if (mIconSize.width() > MIN_ICON_SIZE)
    {
        mIconSize = QSize(MIN_ICON_SIZE, MIN_ICON_SIZE);
        updateUI();

        mFitSwatches = false;
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.setValue("PreferredColorGridSize", MIN_ICON_SIZE);
        settings.setValue("FitSwatchSize", false);
    }
}

void ColorPaletteWidget::setSwatchSizeMedium()
{
    if (mIconSize.width() != MEDIUM_ICON_SIZE)
    {
        mIconSize = QSize(MEDIUM_ICON_SIZE, MEDIUM_ICON_SIZE);
        updateUI();

        mFitSwatches = false;
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.setValue("PreferredColorGridSize", MEDIUM_ICON_SIZE);
        settings.setValue("FitSwatchSize", false);
    }
}

void ColorPaletteWidget::setSwatchSizeLarge()
{
    if (mIconSize.width() < MAX_ICON_SIZE)
    {
        mIconSize = QSize(MAX_ICON_SIZE, MAX_ICON_SIZE);
        updateUI();

        mFitSwatches = false;
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.setValue("PreferredColorGridSize", MAX_ICON_SIZE);
        settings.setValue("FitSwatchSize", false);
    }
}

void ColorPaletteWidget::adjustSwatches()
{
    if (mFitSwatches)
        fitSwatchSize();
}

void ColorPaletteWidget::fitSwatchSize()
{
    int height = ui->colorListWidget->height();
    int width = ui->colorListWidget->width();
    int hScrollBar = ui->colorListWidget->horizontalScrollBar()->geometry().height() + 6;
    int vScrollBar = ui->colorListWidget->verticalScrollBar()->geometry().width() * 2;
    int colorCount = editor()->object()->getColorCount();
    int size;

    if (ui->colorListWidget->viewMode() == QListView::ListMode)
    {
        size = qFloor((height - hScrollBar - (4 * colorCount)) / colorCount);
        if (size < MIN_ICON_SIZE) size = MIN_ICON_SIZE;
        if (size > MAX_ICON_SIZE) size = MAX_ICON_SIZE;
    }
    else
    {
        bool proceed = true;
        size = MIN_ICON_SIZE;
        while (proceed)
        {
            int columns = (width - vScrollBar) / size;
            int rows = static_cast<int>(qCeil(colorCount / columns));
            if (height - hScrollBar > rows * (size + 6))
            {
                size++;
                if (size == MAX_ICON_SIZE)
                {
                    proceed = false;
                }
            }
            else
            {
                proceed = false;
            }
        }
    }
    mIconSize = QSize(size, size);

    updateUI();

    mFitSwatches = true;
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue("PreferredColorGridSize", size);
    settings.setValue("FitSwatchSize", true);
}

void ColorPaletteWidget::resizeEvent(QResizeEvent* event)
{
    updateUI();
    if (mFitSwatches)
    {
        fitSwatchSize();
    }
    QWidget::resizeEvent(event);
}

void ColorPaletteWidget::updateGridUI()
{
    if (ui->colorListWidget->viewMode() == QListView::IconMode) {
        // 找一个能整除面板可用宽、且落在基础宽 ~+8px 内的列宽，
        // 让列恰好铺满（减滚动条宽 18）
        const int baseWidth = mIconSize.width();
        const int available = ui->colorListWidget->width() - 18;
        int effective = baseWidth;
        for (int i = 1; i < 75; i++)
        {
            const int size = available / i;
            if (size >= baseWidth && size <= baseWidth + 8)
            {
                effective = size;
            }
        }

        // 拉伸宽变了 → 位图必须按新宽重建（iconSize 大于位图时委托居中绘制会露缝）。
        // refreshColorList 尾部回访本函数，届时宽度已一致不再重建，无死循环
        if (effective != mStretchedWidth)
        {
            mStretchedWidth = effective;
            refreshColorList();
            return;
        }

        const QSize tile(effective, mIconSize.height());
        ui->colorListWidget->setSpacing(0);
        ui->colorListWidget->setIconSize(tile);
        ui->colorListWidget->setGridSize(tile);
    }
    else
    {
        mStretchedWidth = 0;
        ui->colorListWidget->setIconSize(mIconSize);
        ui->colorListWidget->setGridSize(QSize(-1, -1));
    }
}

void ColorPaletteWidget::clickAddColorButton()
{
    QColor newColor = mEditor->color()->frontColor(false);

    if (!newColor.isValid())
    {
        return; // User canceled operation
    }

    int colorIndex = mObject->getColorCount();
    ColorRef ref(newColor);

    mObject->addColor(ref);
    refreshColorList();

    editor()->color()->setColorNumber(colorIndex);
    editor()->color()->setIndexedColor(ref.color);
    if (mFitSwatches)
    {
        fitSwatchSize();
    }
}

void ColorPaletteWidget::clickRemoveColorButton()
{
    for (auto item : ui->colorListWidget->selectedItems())
    {
        int index = ui->colorListWidget->row(item);

        // items are not deleted by qt, it has to be done manually
        // delete should happen before removing the color from from palette
        // as the palette will be one ahead and crash otherwise

        if (mObject->getColorCount() == 1)
        {
            showPaletteReminder();
            break;
        }

        bool colorRemoved = false;
        if (mObject->getColorCount() > 1)
        {
            delete item;
            mObject->removeColor(index);
            colorRemoved = true;
        }

        if (colorRemoved) {
            int newIndex = qBound(0, index-1, mObject->getColorCount() - 1);
            emit colorNumberChanged(newIndex);
        }
    }
    mMultipleSelected = false;
    if (mFitSwatches)
    {
        fitSwatchSize();
    }
    mEditor->updateFrame();
}

bool ColorPaletteWidget::showPaletteWarning()
{
    QMessageBox msgBox;
    msgBox.setText(tr("The color(s) you are about to delete are currently being used by one or multiple strokes."));
    msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
    QPushButton* removeButton = msgBox.addButton(tr("Delete"), QMessageBox::AcceptRole);

    msgBox.exec();
    if (msgBox.clickedButton() == removeButton)
    {
        if (ui->colorListWidget->selectedItems().size() > 1)
        {
            mMultipleSelected = true;
        }
        return true;
    }
    return false;
}

void ColorPaletteWidget::showPaletteReminder()
{
    QMessageBox::warning(nullptr, tr("Palette Restriction"),
                                  tr("The palette requires at least one swatch to remain functional"));
}

void ColorPaletteWidget::updateItemColor(int itemIndex, QColor newColor)
{
    const QSize tile = swatchTileSize();

    QPixmap colorSwatch(tile);
    QPainter swatchPainter(&colorSwatch);
    swatchPainter.drawTiledPixmap(0, 0, tile.width(), tile.height(), QPixmap(":/background/checkerboard.png"));
    swatchPainter.fillRect(0, 0, tile.width(), tile.height(), newColor);

    // 名称画在底图上：普通态/选中态都可见
    const bool iconMode = ui->colorListWidget->viewMode() == QListView::IconMode;
    if (iconMode)
    {
        drawSwatchName(swatchPainter, tile, newColor, mObject->getColor(itemIndex).name);
    }

    QIcon swatchIcon;
    swatchIcon.addPixmap(colorSwatch, QIcon::Normal);

    if (iconMode)
    {
        // Draw selection border
        QPen borderShadow(QColor(0, 0, 0, 200), 1, Qt::DotLine, Qt::FlatCap, Qt::MiterJoin);
        QVector<qreal> dashPattern;
        dashPattern << 4 << 4;
        borderShadow.setDashPattern(dashPattern);
        QPen borderHighlight(borderShadow);
        borderHighlight.setColor(QColor(255, 255, 255, 200));
        borderHighlight.setDashOffset(4);

        swatchPainter.setPen(borderHighlight);
        swatchPainter.drawRect(0, 0, tile.width() - 1, tile.height() - 1);
        swatchPainter.setPen(borderShadow);
        swatchPainter.drawRect(0, 0, tile.width() - 1, tile.height() - 1);
    }
    swatchIcon.addPixmap(colorSwatch, QIcon::Selected);

    ui->colorListWidget->item(itemIndex)->setIcon(swatchIcon);
    editor()->object()->setColor(itemIndex, newColor);

    // Make sure to update grid in grid mode
    if (ui->colorListWidget->viewMode() == QListView::IconMode)
    {
        updateGridUI();
    }
}
