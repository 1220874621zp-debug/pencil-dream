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
#ifndef LAYERSHADOW_H
#define LAYERSHADOW_H

#include "layerbitmap.h"

#include "graphics/bitmap/shadowengine.h"

class ShadowImage;

/*
 * 智能阴影图层：继承 LayerBitmap，分割线编辑/存取/时间轴曝光块全部
 * 复用位图图层管线。
 *
 * 工作流：用户用标记色（红/黄/蓝，颜色只作标记）逐帧画明暗交界线，
 * 选定方向（阴影在左/右/上/下哪一侧）后触发计算——引擎按方向判定
 * 阴影侧并填充单一阴影色，歧义图形继承空间邻近已判定图形的状态。
 *
 * 线稿源 = 就近的非空位图图层（渲染/计算时解析，同填色层）；
 * 帧内容 = 用户画的分割线（ShadowImage），填充结果为派生缓存。
 */
class LayerShadow : public LayerBitmap
{
    Q_DECLARE_TR_FUNCTIONS(LayerShadow)

public:
    static constexpr int kMarkerSlotCount = 3; // 标记色槽位：红/黄/蓝

    explicit LayerShadow(int id);
    ~LayerShadow() override;

    QDomElement createDomElement(QDomDocument& doc) const override;
    void loadDomElement(const QDomElement& element, QString dataDirPath, ProgressCallback progressStep) override;

    ShadowImage* getShadowImageAtFrame(int frameNumber);
    ShadowImage* getLastShadowImageAtFrame(int frameNumber);

    void replaceKeyFrame(const KeyFrame*) override;

    /** 同步计算指定帧的阴影缓存；structureGeneration = 计算时的图层
     *  结构代数（回填缓存用，恒记 0 会每帧重算不停）。
     *  sourceLayer = 线稿源图层（就近非空位图层，调用方解析） */
    bool updateShadowAtFrame(int frameNumber, LayerBitmap* sourceLayer, quint32 structureGeneration);

    // --- 方向与填充 ---
    ShadowFill::Direction direction() const { return mDirection; }
    void setDirection(ShadowFill::Direction d) { mDirection = d; }

    QRgb fillColor() const { return mFillColor; }
    void setFillColor(QRgb color) { mFillColor = color; }

    qreal gapRadius() const { return mGapRadius; }
    void setGapRadius(qreal r) { mGapRadius = r; }

    // --- 标记色（三槽：红/黄/蓝默认，可换色、可标"透明"=禁用线） ---
    QRgb markerColor(int slot) const;
    void setMarkerColor(int slot, QRgb color);
    bool markerTransparent(int slot) const { return slot >= 0 && slot < kMarkerSlotCount && mMarkerTransparent[slot]; }
    void setMarkerTransparent(int slot, bool transparent);
    /** 被标"透明"的标记色 = 禁用线颜色（喂给引擎） */
    QVector<QRgb> disabledMarkerColors() const;

    // --- 显示/编辑模式（对齐填色层 Krita 语义） ---
    /** 编辑模式：显示并可绘制分割线；关闭=只看阴影结果 */
    bool editLines() const { return mEditLines; }
    void setEditLines(bool v) { mEditLines = v; }

    /** 显示阴影填充结果 */
    bool showFill() const { return mShowFill; }
    void setShowFill(bool v) { mShowFill = v; }

protected:
    KeyFrame* createKeyFrame(int position) override;
    void loadImageAtFrame(QString strFilePath, QPoint topLeft, int frameNumber, qreal opacity) override;

private:
    ShadowFill::Direction mDirection = ShadowFill::DirLeft;
    QRgb mFillColor = qRgb(128, 128, 128); // 默认中灰：黑色色块在深色面板上看不出
    qreal mGapRadius = 4.0;
    QRgb mMarkerColors[kMarkerSlotCount] = { qRgb(255, 0, 0), qRgb(255, 255, 0), qRgb(0, 0, 255) };
    bool mMarkerTransparent[kMarkerSlotCount] = { false, false, false };
    bool mEditLines = true;
    bool mShowFill = true;
};

#endif // LAYERSHADOW_H
