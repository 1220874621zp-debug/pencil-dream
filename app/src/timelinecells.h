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

#ifndef TIMELINECELLS_H
#define TIMELINECELLS_H

#include <QString>
#include <QHash>
#include <QPixmap>
#include <QSet>
#include <QWidget>
#include <functional>
#include "layercamera.h"

#include "object.h"

class Layer;
class LayerVideo;
class QLineEdit;
enum class LayerVisibility;
class SoundClip;
class TimeLine;
class QPaintEvent;
class QMouseEvent;
class QResizeEvent;
class Editor;
class PreferenceManager;
class QMenu;
class QAction;
class QTimer;
enum class SETTING;

enum class TIMELINE_CELL_TYPE
{
    Layers,
    Tracks
};

class TimeLineCells : public QWidget
{
    Q_OBJECT

public:
    TimeLineCells( TimeLine* parent, Editor* editor, TIMELINE_CELL_TYPE );
    ~TimeLineCells() override;

    static int getOffsetX() { return mOffsetX; }
    static int getOffsetY() { return mOffsetY; }
    int getLayerHeight() const { return mLayerHeight; }
    /** 可见行数（含组头行；滚动条范围用） */
    int visibleRowCount() const { rebuildRows(); return mRows.size(); }
    /** 层在可视行序列中从上往下的行号；-1 = 藏在收起组内（滚动定位用） */
    int visualRowFromTop(int layerNumber) const;

    int getFrameLength() const { return mFrameLength; }
    int getFrameSize() const { return mFrameSize; }

    void setFrameLength(int n) { mFrameLength = n; }
    void setFrameSize(int size);
    void setLayerHeight(int h);
    void setLayerCollapsed(int layerId, bool collapsed);
    void clearCache() { delete mCache; mCache = nullptr; }

    bool didDetachLayer() const;

    void showCameraMenu(QPoint pos);

signals:
    void mouseMovedY(int);
    /** 图层行右键菜单请求删除指定层（走 TimeLine::deleteCurrentLayerClick 确认链） */
    void deleteLayerRequested(int layerIndex);
    void mergeDownRequested(int layerIndex);
    void lengthChanged(int);
    void offsetChanged(int);
    void selectionChanged();
    void insertNewKeyFrame();
    void layerHeightChanged(int h);
    void layerCollapsedChanged(int layerId, bool collapsed);
    void frameSizeChanged(int size);

public slots:
    void updateContent();
    void updateFrame(int frameNumber);
    void hScrollChange(int);
    void vScrollChange(int);
    void onScrollingVerticallyStopped();
    void setMouseMoveY(int x);

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private slots:
    void loadSetting(SETTING setting);
    void processThumbQueue();

private:
    int getLayerNumber(int y) const;
    int getInbetweenLayerNumber(int y) const;
    int getLayerY(int layerNumber) const;
    int rowHeightOf(int layerNumber) const;
    bool isLayerCollapsed(int layerNumber) const;
    void toggleLayerCollapsed(int layerNumber);
    int getFrameX(int frameNumber) const;
    int getFrameNumber(int x) const;

    // ---- 组头行模型（组头+图层行；与轨道区两列同源同序） ----
    void rebuildRows() const;
    void rebuildRowPrefix() const;                   // 行高前缀和（rowYAt 的 O(1) 化）
    int rowHeightAt(int rowIndex) const;             // 行高（组头=22，成员按层折叠态）
    int rowYAt(int rowIndex) const;                  // 行顶 y
    int rowIndexAtY(int y) const;                    // 命中行号；-1 = 首行之上
    int rowIndexOfLayer(int layerNumber) const;      // 层所在行；收起组内成员 = 其组头行
    /** 点在组头行上返回 gid（须在通用命中之前调用），否则 -1 */
    int headerGroupIdAt(const QPoint& pos) const;
    /** 该层的行是否因组收起而不可见 */
    bool layerRowHidden(const Layer* layer) const;
    /** 拖拽成组悬停判定：单层拖动落在目标行（层/组头）中心区时为真，rowOut=目标行 */
    bool groupDropHoverRow(int& rowOut) const;
    void paintGroupHeader(QPainter& painter, int groupId, int x, int y, int width, int height) const;
    void paintGroupTrack(QPainter& painter, int groupId, int y, int height) const;
    void showGroupHeaderMenu(QPoint pos, int groupId);
    void showLayerGroupMenu(QPoint pos, int layerIndex); // 图层右键：成组入口
    /** 组成员帧范围带（Tracks 组头行）：[minKeyPos, maxEnd]；空组返回空 */
    QPair<int, int> groupFrameExtent(int groupId) const;

    mutable QList<Object::TimelineRowRef> mRows;
    mutable quint64 mRowsStamp = 0;
    mutable QList<int> mRowPrefixHeights; // P[i]=第0..i-1行行高之和；空=待重建
    int mGroupDragId = -1;        // 正在整组拖动的 gid（释放时换算插入目标）

    // Dreams-style block helpers
    /** Width of the exposure block of the given keyframe, in frames (trim preview aware). */
    int blockLengthFor(const Layer* layer, const KeyFrame* key) const;
    /** Camera key dot diameter in px (shared by paintFrames/paintSelectedFrames
     *  so the selection ring always hugs the drawn dot). */
    qreal cameraKeyDotDiameter(int recHeight) const;
    /** Returns the keyframe pos whose block's right edge is under the given position, or -1. */
    int hitTestTrimHandle(const QPoint& pos) const;
    /** Returns the layer index whose trailing "+" handle is under the position, or -1. */
    int hitTestPlusHandle(const QPoint& pos) const;
    void paintPlusPreview(QPainter& painter) const;
    QPixmap thumbnailFor(const Layer* layer, int framePos) const;
    /** Move the selected frames of the source layer to the target layer (same type only).
     *  Returns false (without touching anything) when the drop is impossible. */
    bool moveSelectedFramesAcrossLayers(int sourceIndex, int targetIndex);

    void onDidLeaveWidget();

    bool trackScrubber();
    void drawContent();
    void paintTicks(QPainter& painter, const QPalette& palette) const;
    void paintOnionSkin(QPainter& painter) const;
    void paintLayerGutter(QPainter& painter) const;
    void paintTrack(QPainter& painter, const Layer* layer, int x, int y, int width, int height, bool selected, int frameSize) const;
    void paintCollapsedTrack(QPainter& painter, const Layer* layer, int x, int y, int width) const;
    void drawCollapseTriangle(QPainter& painter, const Layer* layer, int x, int y, int width, int height) const;
    void paintFrames(QPainter& painter, QColor trackCol, const Layer* layer, int y, int height, bool selected, int frameSize) const;
    /** Camera key dots + connector line (shared by paintFrames and the
     *  hover-reveal over the track-start camera icon). */
    void paintCameraKeys(QPainter& painter, const Layer* layer, int y, int height) const;
    void paintCurrentFrameBorder(QPainter& painter, int recLeft, int recTop, int recWidth, int recHeight) const;
    void paintSoundWaveform(QPainter& painter, SoundClip* clip, int recLeft, int recTop, int recWidth, int recHeight) const;
    void paintVideoBand(QPainter& painter, const Layer* layer, int recLeft, int recTop, int recWidth, int recHeight) const;

    // ---- 参考视频层属性展开区(friction 式:行内展开,自绘控件) ----
    static constexpr int kVideoPropsH = 66;   // 三行 × 22
    // 命中字段:0=无 1=缩放滑杆 2=缩放数值 3=位移X数值 4=位移Y数值
    int  hitVideoProps(int layerNumber, const QPoint& pos) const;
    void paintVideoProps(QPainter& painter, const LayerVideo* layer, int x, int yTop) const;
    void setVideoPropsValue(LayerVideo* layer, int layerNumber, int field, double v);
    // 不透明度拖动防抖:拖动中只记值,80ms 停顿/松手才落板画布
    void scheduleOpacityApply(int layerNumber, qreal value);
    void flushPendingOpacity(int layerNumber);
    void toggleVideoPropsExpanded(int layerNumber);
    void openVideoPropsEditor(int layerNumber, int field);
    QRect videoPropsFieldRect(int layerNumber, int field) const;
    void paintSelectedFrames(QPainter& painter, const Layer* layer, const int layerIndex) const;
    void paintLabel(QPainter& painter, const Layer* layer, int x, int y, int width, int height, bool selected, LayerVisibility layerVisibility) const;
    void paintSelection(QPainter& painter, int x, int y, int width, int height) const;
    void paintHighlightedFrame(QPainter& painter, int framePos, int recTop, int recWidth, int recHeight) const;

    void editLayerProperties(Layer* layer) const;
    void editLayerProperties(LayerCamera *layer) const;
    void editLayerName(Layer* layer) const;

    // TVP layer-row inline controls: opacity slider + lock toggle + clip toggle
    QRect opacitySliderRect(int rowWidth) const;
    QRect lockIconRect(int rowWidth) const;
    QRect clipIconRect(int rowWidth) const;
    bool rowHasInlineControls(int rowWidth) const;

    TimeLine* mTimeLine;
    Editor* mEditor; // the editor for which this timeLine operates
    PreferenceManager* mPrefs;

    TIMELINE_CELL_TYPE mType;

    QPixmap* mCache = nullptr;
    mutable QHash<qint64, QPixmap> mThumbCache;  // key = layerId<<32 | framePos
    mutable QList<qint64> mThumbLru;             // 末尾=最近使用，驱逐从头取
    // async thumbnail generation: misses are queued and rendered in batches
    struct ThumbRequest { int layerId; int framePos; };
    mutable QList<ThumbRequest> mThumbQueue;
    mutable QSet<qint64> mThumbQueued;
    // 图层行内图标（类型/clip/循环徽章）的一次性栅格化缓存：原来每行每次
    // 重绘都从资源重新加载 SVG 并缩放/染色
    mutable QHash<QString, QPixmap> mRowIconCache;
    QPixmap cachedRowIcon(const QString& key, const std::function<QPixmap()>& make) const;
    QTimer* mThumbTimer = nullptr;
    QSet<int> mCollapsedLayerIds;
    bool mRedrawContent = false;
    bool mDrawFrameNumber = true;
    bool mbShortScrub = false;
    int mFrameLength = 1;
    int mFrameSize = 0;
    int mFontSize = 11;
    bool mScrubbing = false;
    bool mHighlightFrameEnabled = false;
    int mHighlightedFrame = -1;
    int mLayerHeight = 72;
    int mStartY = 0;
    int mEndY   = 0;

    int mCurrentLayerNumber = 0;
    int mLastScrubFrame = 0;

    int mFromLayer = 0;
    int mToLayer   = 1;
    int mStartLayerNumber = -1;
    int mStartFrameNumber = 0;
    int mLastFrameNumber = -1;

    // is used to move layers, don't use this to get mousePos;
    int mMouseMoveY = 0;
    int mPrevFrame = 0;
    int mFrameOffset = 0;
    int mLayerOffset = 0;
    Qt::MouseButton primaryButton = Qt::NoButton;

    bool mScrollingVertically = false;

    bool mCanMoveFrame   = false;
    bool mMovingFrames   = false;

    // Dreams-style trim (drag block right edge)
    bool mTrimming = false;
    Layer* mTrimLayer = nullptr; // trim 预览只作用于被拖拽的层，防止跨层泄漏
    int mTrimKeyPos = -1;
    int mTrimOriginalLength = 1;
    int mTrimPreviewLength = 1;
    int mTrimRippleOffset = 0; // live shift (frames) applied to later blocks while trimming

    // Whole-layer grab (Ctrl + drag a block moves every frame of the layer)
    bool mWholeLayerMode = false;

    // Trailing "+" handle drag-create (TVP-style)
    bool mPlusCreating = false;
    int mPlusPreviewCount = 0;

    // Cross-layer drag & drop
    int mDropTargetLayer = -1;
    int mDropShiftFrames = 0;

    bool mCanBoxSelect   = false;
    bool mBoxSelecting   = false;

    bool mClickSelecting = false;

    // layer-row opacity slider drag (index of the dragged layer, -1 = none)
    int mOpacityDragLayer = -1;

    int mFramePosMoveX = 0;
    int mLayerPosMoveY = 0;

    // 不透明度拖动防抖(Krita LayerBox 同款 KisSignalCompressor 模式):
    // move 只记值+行重绘(手柄跟手),画布应用延迟到拖动停顿,松手立即落地
    QTimer* mOpacityApplyTimer = nullptr;
    int mPendingOpacityLayer = -1;
    qreal mPendingOpacity = 1.0;

    // 视频层属性展开态(层id;会话级UI态,不存盘)
    QSet<int> mExpandedVideoIds;
    // 属性区拖动/编辑状态
    int mVideoPropsDragField = 0;
    QPoint mVideoPropsPressPos;
    double mVideoPropsPressVal = 0.0;
    bool mVideoPropsDragging = false;
    QLineEdit* mVideoPropsEditor = nullptr;

    int mMouseMoveX = 0;
    int mMousePressX = 0;
    QPoint mMousePos = QPoint(-1000, -1000); // 当前悬停位置（离开部件后置回屏外）

    const static int mOffsetX = 0;
    const static int mOffsetY = 28;
    const static int mLayerDetachThreshold = 5;

};

#endif // TIMELINECELLS_H
