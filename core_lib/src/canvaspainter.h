/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef CANVASPAINTER_H
#define CANVASPAINTER_H

#include <memory>
#include <QCoreApplication>
#include <QObject>
#include <QTransform>
#include <QPainter>
#include <QPainterPath>
#include "log.h"
#include "pencildef.h"

#include "layer.h"

#include "onionskinpainteroptions.h"
#include "onionskinsubpainter.h"


class TiledBuffer;
class Object;
class BitmapImage;
class ViewManager;

/** 洋葱皮幽灵的单侧运行时变换（对位中割用，不落盘）。
 *  平移 + 绕幽灵内容包围盒中心旋转 + 等比缩放；identity 时走纯平移快路径。 */
struct OnionGhostTransform
{
    QPointF offset;
    qreal rotation = 0.0;  ///< 度，正值顺时针（画布 y 向下坐标系）
    qreal scale = 1.0;

    bool isPlainTranslation() const { return qFuzzyIsNull(rotation) && qFuzzyCompare(scale, 1.0); }
};

/** 洋葱皮幽灵的运行时显示变换（对位中割用，不落盘）。
 *  变换绑定创建时的洋葱帧号：换帧后前后幽灵变了，旧变换自动失效归零。 */
struct OnionGhostOffset
{
    OnionGhostTransform prev;
    OnionGhostTransform next;
    int prevFrameNumber = -1;
    int nextFrameNumber = -1;
};
using OnionGhostOffsetMap = QMap<int, OnionGhostOffset>;

/** 穿透模式：拖拽预览变换（画布坐标系）。
 *  p ↦ scaleAnchor + translation + (p − scaleAnchor)·(scaleX, scaleY)；
 *  提交时同一数学用 QTransform 烤进位图像素。 */
struct XrayDragPreview
{
    bool active = false;
    int layerId = -1;        ///< 目标图层 id（当前位图层）
    int framePos = -1;       ///< 目标关键帧位
    QPointF translation;
    qreal scaleX = 1.0;
    qreal scaleY = 1.0;
    QPointF scaleAnchor;

    bool isIdentity() const
    {
        return translation.isNull() && qFuzzyCompare(scaleX, 1.0) && qFuzzyCompare(scaleY, 1.0);
    }
};

/** 穿透模式画布显示态（ScribbleArea 持有，渲染只读） */
struct XrayVisualState
{
    bool enabled = false;
    int selectedFrame = -1;             ///< 当前层被选中的幽灵关键帧位（提亮+手柄）
    XrayDragPreview drag;
};

struct CanvasPainterOptions
{
    bool  bAntiAlias = false;
    bool  bThinLines = false;
    bool  bOutlines = false;

    LayerVisibility eLayerVisibility = LayerVisibility::RELATED;
    float fLayerVisibilityThreshold = 0.f;
    bool bOnionSkinMultiLayer = false;
    float scaling = 1.0f;
    QPainter::CompositionMode cmBufferBlendMode = QPainter::CompositionMode_SourceOver;
    OnionSkinPainterOptions mOnionSkinOptions;
};

class CanvasPainter
{
    Q_DECLARE_TR_FUNCTIONS(CanvasPainter)
public:
    explicit CanvasPainter(QPixmap& canvas);
    virtual ~CanvasPainter();

    void reset();
    void setViewTransform(const QTransform view, const QTransform viewInverse);

    void setOnionSkinOptions(const OnionSkinPainterOptions& onionSkinOptions) { mOnionSkinPainterOptions = onionSkinOptions;}
    void setOptions(const CanvasPainterOptions& p) { mOptions = p; }
    /** 洋葱皮对位工具：传入按图层 id 索引的幽灵偏移表（调用方保证生命周期） */
    void setOnionGhostOffsets(const OnionGhostOffsetMap* offsets) { mOnionGhostOffsets = offsets; }
    /** 穿透模式：传入显示态（模式开关/选中帧/拖拽预览，调用方保证生命周期） */
    void setXrayState(const XrayVisualState* state) { mXrayState = state; }
    void setTransformedSelection(QRect selection, QTransform transform, QPolygonF selectionPolygon = QPolygonF());
    void ignoreTransformedSelection();

    /** Deform tool live preview: while active the selection area is cleared
     *  and the warped image is drawn instead of the affine-transformed one.
     *  targetRect is in canvas coordinates and may stretch the preview. */
    void setDeformPreview(const QImage& preview, const QRectF& targetRect);
    void clearDeformPreview();
    /** Test seam: the current live-preview image (null when inactive). */
    const QImage& deformPreview() const { return mDeformPreview; }

    /** Clip applied to in-progress stroke tiles (selection constraint). */
    void setSelectionClipPath(const QPainterPath& path) { mSelectionClipPath = path; }

    void setPaintSettings(const Object* object, int currentLayer, int frame, TiledBuffer* tilledBuffer);
    void paint(const QRect& blitRect);
    void paintCached(const QRect& blitRect);
    void resetLayerCache();
    /** 单侧失效:层显示参数(如参考视频缩放/位移)变化时,
     *  只重画该层所在的缓存块,避免波及另一侧全部层 */
    void resetPreLayerCache() { mPreLayersPixmapCacheValid = false; }
    void resetPostLayerCache() { mPostLayersPixmapCacheValid = false; }

private:

    /**
     * CanvasPainter::initializePainter
     * Enriches the painter with a context and sets it's initial matrix.
     * @param painter The in/out painter
     * @param pixmap The paint device ie. a pixmap
     * @param blitRect The rect where the blitting will occur
     */
    void initializePainter(QPainter& painter, QPaintDevice& device, const QRect& blitRect);

    void paintOnionSkinOnLayer(QPainter& painter, const QRect& blitRect, Layer* layer);
    void paintOnionSkin(QPainter& painter, const QRect& blitRect);

    void renderPostLayers(QPainter& painter, const QRect& blitRect);
    void renderPreLayers(QPainter& painter, const QRect& blitRect);

    void paintCurrentFrame(QPainter& painter, const QRect& blitRect, int startLayer, int endLayer);

    void paintTransformedSelection(QPainter& painter, BitmapImage* bitmapImage, const QRect& selection) const;

    void paintBitmapOnionSkinFrame(QPainter& painter, const QRect& blitRect, Layer* layer, int nFrame, bool colorize);
    void paintOnionSkinFrame(QPainter& painter, QPainter& onionSkinPainter, int nFrame, bool colorize, qreal frameOpacity);
    OnionGhostTransform onionGhostTransform(const Layer* layer, int nFrame) const;

    /** 穿透模式：当前位图层全部关键帧幽灵（叠加显示，激活时取代洋葱皮） */
    void paintXrayFrames(QPainter& painter, const QRect& blitRect, Layer* layer);

    bool paintCurrentBitmapFrame(QPainter& painter, const QRect& blitRect, Layer* layer, bool isCurrentLayer, QImage* clipMask = nullptr);

    /** 智能填色层：着色缓存 + 笔画（含当前层实时笔画缓冲） */
    void paintCurrentColorizeFrame(QPainter& painter, const QRect& blitRect, Layer* layer, int layerIndex, bool isCurrentLayer);
    void paintVideoFrame(QPainter& painter, Layer* layer);

    // --- clipping-mask compositing ----------------------------------------
    /** (Re)creates the accumulated-below image to match the canvas geometry. */
    void ensureClipAccum();
    /** Clears the accumulated-below image in the blit area (device-space 1:1). */
    void clearClipAccum(const QRect& blitRect);
    /** Mirrors the finished layer content (device space) into the accumulator,
     *  applying the same opacity the target blit used. */
    void clipAccumulate(const QPixmap& layerContent, qreal opacity);

    CanvasPainterOptions mOptions;

    const Object* mObject = nullptr;
    QPixmap& mCanvas;
    QTransform mViewTransform;
    QTransform mViewInverse;

    int mCurrentLayerIndex = 0;
    int mFrameNumber = 0;
    TiledBuffer* mTiledBuffer = nullptr;

    QImage mScaledBitmap;

    // Handle selection transformation
    bool mRenderTransform = false;
    QRect mSelection;
    QTransform mSelectionTransform;
    QPolygonF mSelectionPolygon;

    // Deform tool preview channel
    bool mDeformPreviewActive = false;
    QImage mDeformPreview;
    QRectF mDeformPreviewTargetRect;

    // selection constraint for live stroke tiles
    QPainterPath mSelectionClipPath;

    // Caches specifically for when drawing on the canvas
    QPixmap mPostLayersPixmap;
    QPixmap mPreLayersPixmap;
    QPixmap mCurrentLayerPixmap;
    QPixmap mOnionSkinPixmap;
    bool mPreLayersPixmapCacheValid = false;
    bool mPostLayersPixmapCacheValid = false;

    // --- clipping-mask compositing state ----------------------------------
    // Only active when at least one bitmap layer has clipMask enabled; the
    // pre/current/post pixmap split above spans layer ranges, so the clip
    // base is tracked in a dedicated image (friction preserve-alpha
    // semantics: the NEAREST non-clip bitmap layer below, replaced each
    // time such a layer renders; clipped layers never write into it).
    bool mAnyClipMask = false;             // fast-path switch, from setPaintSettings
    QImage mClipAccum;                     // clip base content (device space)
    QImage mClipAccumAfterPre;             // snapshot taken when the pre phase finishes
    bool mClipAfterPreValid = false;

    // There's a considerable amount of overhead in simply allocating a QPointF on the fly.
    // Since we just need to draw it at 0,0, we might as well make a const value for that purpose
    const QPointF mPointZero;


    OnionSkinSubPainter mOnionSkinSubPainter;
    OnionSkinPainterOptions mOnionSkinPainterOptions;

    // 洋葱皮对位工具的幽灵偏移（所有权在 ScribbleArea，这里只读）
    const OnionGhostOffsetMap* mOnionGhostOffsets = nullptr;

    // 穿透模式显示态（所有权在 ScribbleArea，这里只读）
    const XrayVisualState* mXrayState = nullptr;

    const static int OVERLAY_SAFE_CENTER_CROSS_SIZE = 25;
};

#endif // CANVASRENDERER_H
