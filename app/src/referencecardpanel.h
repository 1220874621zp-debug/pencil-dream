/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License.
version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef REFERENCECARDPANEL_H
#define REFERENCECARDPANEL_H

#include "basedockwidget.h"

#include <QColor>
#include <QImage>
#include <QPair>
#include <QPointF>
#include <QSet>
#include <QString>
#include <QVector>
#include <QWidget>

class Editor;
class QPushButton;
class QSpinBox;

/** 色卡色块：矩形+上方名称，悬浮在设定图上，pos 为图像像素坐标锚点 */
struct RefColorSwatch
{
    QColor  color;
    QString name;
    QPointF pos;
};

/** 标记折线：点列为图像像素坐标 */
struct RefMarkerLine
{
    QVector<QPointF> points;
};

/** 设定卡片画布：设定图缩放平移 + 色块/标记线自绘与交互，坐标恒存图像像素坐标 */
class ReferenceCardCanvas : public QWidget
{
    Q_OBJECT
public:
    explicit ReferenceCardCanvas(QWidget* parent = nullptr);

    void setEditor(Editor* editor) { mEditor = editor; }
    void setMarkerMode(bool marker);

    bool loadImage(const QString& path, int extractCount = 8);  // 读图，旁边有 .setcard.json 则自动恢复
    void extractSwatches(int count);            // 重新提取色卡（替换色块，保留标记线）
    bool importPreset(const QString& jsonPath); // 导入预设数据文件
    void fitToWindow();

    static QString sidecarPathFor(const QString& imagePath);

    bool hasImage() const { return !mImage.isNull(); }

signals:
    void requestImport();               // 空白菜布右键「导入图片」
    void imageChanged(bool hasImage);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QPointF imageToWidget(const QPointF& p) const { return p * mScale + mPan; }
    QPointF widgetToImage(const QPointF& p) const { return (p - mPan) / mScale; }

    QRectF swatchRect(int index) const;    // widget 坐标（尺寸固定屏幕像素）
    QRectF labelRect(int index) const;
    int    hitSwatch(const QPointF& widgetPos) const;   // 顶层优先，-1 = 未命中
    int    hitLine(const QPointF& widgetPos) const;     // 距折线 < 6px，-1 = 未命中
    void   deleteSelected();
    void   showContextMenu(const QPointF& pos, const QPoint& globalPos);
    void   confirmDraft();
    void   updateCursor(const QPointF& pos);

    QString sidecarPath() const;
    void    saveSidecar();
    bool    loadSidecarData(const QString& jsonPath, bool restoreView);

    void ensureScaledCache();

    Editor* mEditor = nullptr;
    QImage  mImage;
    QString mImagePath;

    QVector<RefColorSwatch> mSwatches;
    QVector<RefMarkerLine>  mLines;

    qreal   mScale = 1.0;
    QPointF mPan { 0.0, 0.0 };

    QPixmap mScaledCache;
    qreal   mCacheScale = -1.0;
    qreal   mCacheDpr = -1.0;

    bool mMarkerMode = false;

    // 色块拖动/点击判别（多选时整组一起拖）
    int     mPressIndex = -1;
    bool    mDragging = false;
    QPointF mPressPos;
    QPointF mGrabOffset;   // 按下时光标（图坐标）相对色块锚点偏移
    QVector<QPair<int, QPointF>> mDragOrigins;   // 拖动起点快照（索引→图坐标）

    // 左键框选（选择模式）/ 中键平移
    QSet<int> mSelected;
    bool    mRubberActive = false;
    QPointF mRubberStart;
    QPointF mRubberCur;
    bool    mPanning = false;
    QPointF mPanPressPos;
    QPointF mPanPressPan;

    // 标记线草稿与悬停位置（橡皮筋预览）
    QVector<QPointF> mDraftPoints;
    QPointF mHoverPos { -1.0, -1.0 };

    int mHoverSwatch = -1;
};

/** 设定卡片面板：导入设定图/预设、提取色卡、选择与标记线工具切换 */
class ReferenceCardPanel : public BaseDockWidget
{
    Q_OBJECT
public:
    explicit ReferenceCardPanel(QWidget* parent = nullptr);

    void initUI() override;
    void updateUI() override {}

private:
    void importImage();
    void importPreset();
    void reextractSwatches();

    ReferenceCardCanvas* mCanvas = nullptr;
    QPushButton* mImportButton = nullptr;
    QPushButton* mPresetButton = nullptr;
    QPushButton* mExtractButton = nullptr;
    QPushButton* mSelectToolButton = nullptr;
    QPushButton* mMarkerToolButton = nullptr;
    QPushButton* mFitButton = nullptr;
    int mLastCount = 8;   // 上次提取数量（弹窗默认值）
};

#endif // REFERENCECARDPANEL_H
