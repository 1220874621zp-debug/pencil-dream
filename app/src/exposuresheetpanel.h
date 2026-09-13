/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License;
version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef EXPOSURESHEETPANEL_H
#define EXPOSURESHEETPANEL_H

#include "basedockwidget.h"

#include <QAbstractScrollArea>
#include <QVector>

class Editor;
class Layer;
class QPushButton;

/** 律表列内一个关键帧（绘张/相机key）的绘制数据 */
struct SheetKeyEntry
{
    int  pos = 0;
    int  number = 0;        // 层内张数序号（按帧位升序 1,2,3…）
    bool keyDrawing = true; // 原画=true（圆圈数字） 中割=false（点）
    int  blockEnd = -1;     // 曝光块独占尾帧；-1=开放尾块（虚线画到表尾）
};

/** 律表一列 = 一个图层（位图族或相机层） */
struct SheetColumn
{
    int     layerId = -1;
    bool    isCamera = false;
    QString name;
    bool    visible = true;
    qreal   opacity = 1.0;
    QVector<SheetKeyEntry> keys;
};

/** 摄影表主体：纵向=帧、横向=图层列的自绘网格。
 *  单视口 QAbstractScrollArea，帧号栏/列头以不透明覆盖钉在视口左缘/上缘。 */
class ExposureSheetView : public QAbstractScrollArea
{
    Q_OBJECT
public:
    explicit ExposureSheetView(QWidget* parent = nullptr);

    void setEditor(Editor* editor) { mEditor = editor; }

    void rebuildColumns();              // 层结构/帧数据/时长变化：重建列与滚动范围
    void updateCurrentFrame(int frame); // 播放头移动：高亮+跟随滚动（不重建）
    void refreshHighlight();            // 当前层/帧变化但数据未变：仅重绘

signals:
    /** 当前(层,帧)的关键帧状态：hasKey=false 表示无关键帧（切换按钮置灰） */
    void toggleTargetChanged(bool hasKey, bool isKeyDrawing);

public slots:
    void toggleCurrentKeyDrawing();     // 面板按钮入口：当前层当前帧（走撤销栈）

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    int  frameRow(int frame) const;      // 帧行顶 y（内容坐标）
    int  rowForY(int y) const;           // y → 帧号（可能越界，调用方校验）
    int  contentWidth() const;
    int  contentHeight() const;
    void updateScrollRanges();
    void ensureFrameVisible(int frame);
    void updateToggleTarget();
    void toggleKeyDrawingAt(Layer* layer, int pos);
    Layer* columnLayer(int index) const;

    Editor* mEditor = nullptr;

    QVector<SheetColumn> mColumns;
    int mFrameCount = 48;
    int mFps = 12;
    int mCurrentFrame = 1;
    int mCurrentLayerId = -1;
    int mHoverEyeColumn = -1; // 列头眼睛悬浮高亮的列
};

/** 摄影表（律表）面板：顶部“原画/中割”切换开关 + 律表主体 */
class ExposureSheetPanel : public BaseDockWidget
{
    Q_OBJECT
public:
    explicit ExposureSheetPanel(QWidget* parent = nullptr);

    void initUI() override;
    void updateUI() override;

private:
    void syncToggleButton(bool hasKey, bool isKeyDrawing);

    ExposureSheetView* mView = nullptr;
    QPushButton* mToggleKeyButton = nullptr;
    bool mSyncingButton = false;
};

#endif // EXPOSURESHEETPANEL_H
