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
#ifndef LAYERCOLORIZE_H
#define LAYERCOLORIZE_H

#include "layerbitmap.h"
#include "graphics/bitmap/colorizeengine.h"

class ColorizeImage;

/** 后台计算任务快照：主线程构建，工作线程消费（QImage 拷贝即隔离） */
struct ColorizeJobData
{
    int layerId = 0;
    int keyPos = 0;
    QRect bounds;
    QImage lineImg;
    QImage strokeImg;
    Colorize::FilteringOptions options;
};

/*
 * 智能填色图层（Krita「智能填色蒙版」移植）：继承 LayerBitmap，
 * 笔画编辑/存取/时间轴曝光块全部复用位图图层管线。
 *
 * 线稿源 = 上方最近的位图图层（渲染/计算时解析）；
 * 帧内容 = 用户画的彩色笔画（ColorizeImage），着色结果为派生缓存。
 */
class LayerColorize : public LayerBitmap
{
    Q_DECLARE_TR_FUNCTIONS(LayerColorize)

public:
    explicit LayerColorize(int id);
    ~LayerColorize() override;

    QDomElement createDomElement(QDomDocument& doc) const override;
    void loadDomElement(const QDomElement& element, QString dataDirPath, ProgressCallback progressStep) override;

    ColorizeImage* getColorizeImageAtFrame(int frameNumber);
    ColorizeImage* getLastColorizeImageAtFrame(int frameNumber);

    void replaceKeyFrame(const KeyFrame*) override;

    /** 同步计算指定帧的着色缓存（手动/导出/批量路径）；structureGeneration
     *  = 计算时的图层结构代数，回填缓存，否则恒记 0 会每帧重算不停 */
    bool updateColoringAtFrame(int frameNumber, LayerBitmap* sourceLayer, quint32 structureGeneration);

    /** 后台任务快照构建：组装计算域与输入图；无需计算时返回 false */
    static bool buildColorizeJob(LayerColorize* layer, int frameNumber,
                                 LayerBitmap* sourceLayer, ColorizeJobData& out);

    // --- 滤波选项（对齐 Krita Colorize Mask 参数） ---
    bool useEdgeDetection() const { return mUseEdgeDetection; }
    void setUseEdgeDetection(bool v) { mUseEdgeDetection = v; }

    qreal edgeDetectionSize() const { return mEdgeDetectionSize; }
    void setEdgeDetectionSize(qreal v) { mEdgeDetectionSize = v; }

    qreal fuzzyRadius() const { return mFuzzyRadius; }
    void setFuzzyRadius(qreal v) { mFuzzyRadius = v; }

    qreal cleanUpAmount() const { return mCleanUpAmount; }
    void setCleanUpAmount(qreal v) { mCleanUpAmount = v; }

    // --- 显示/编辑模式（对齐 Krita colorizeEditKeyStrokes / colorizeShowColoring） ---
    /** 编辑模式：显示并可绘制笔画；关闭=只看着色结果（Krita: Edit key strokes） */
    bool editKeyStrokes() const { return mEditKeyStrokes; }
    void setEditKeyStrokes(bool v) { mEditKeyStrokes = v; }

    /** 显示填色结果（Krita: Show output） */
    bool showColoring() const { return mShowColoring; }
    void setShowColoring(bool v) { mShowColoring = v; }

    // --- 透明颜色（Krita transparentIndex：该颜色的笔画区域保持不填） ---
    bool hasTransparentColor() const { return mHasTransparentColor; }
    QRgb transparentColor() const { return mTransparentColor; }
    void setTransparentColor(QRgb color);
    void clearTransparentColor() { mHasTransparentColor = false; }

    /** 从指定帧的笔画图里删除某颜色的全部笔画（Krita: Remove） */
    void removeStrokeColor(int frameNumber, QRgb color);

    /** 当前帧使用的笔画颜色列表（非预乘，按面积降序） */
    QVector<QRgb> strokeColorsAtFrame(int frameNumber);

protected:
    KeyFrame* createKeyFrame(int position) override;
    void loadImageAtFrame(QString strFilePath, QPoint topLeft, int frameNumber, qreal opacity) override;

private:
    bool mUseEdgeDetection = false;
    qreal mEdgeDetectionSize = 4.0;
    qreal mFuzzyRadius = 0.0;
    qreal mCleanUpAmount = 0.7;
    bool mEditKeyStrokes = true;
    bool mShowColoring = true;
    bool mHasTransparentColor = false;
    QRgb mTransparentColor = 0;
};

#endif // LAYERCOLORIZE_H
