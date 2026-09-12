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
#include "onionskinsubpainter.h"

#include <QPainter>

#include "keyframe.h"
#include "layer.h"
#include "onionskinpainteroptions.h"

OnionSkinSubPainter::OnionSkinSubPainter()
{
}

void OnionSkinSubPainter::paint(QPainter& painter, const Layer* layer, const OnionSkinPainterOptions& options, int frameIndex, const std::function<void(OnionSkinPaintState, int)>& state) const
{
    if (!options.enabledWhilePlaying && options.isPlaying) { return; }

    if (layer->visible() == false)
        return;

    if (layer->keyFrameCount() == 0)
        return;

    // 循环层洋葱皮在显示帧域工作（回绕到周期内再取邻帧）
    frameIndex = layer->displayFrameFor(frameIndex);

    qreal minOpacity = static_cast<qreal>(options.minOpacity / 100);
    qreal maxOpacity = static_cast<qreal>(options.maxOpacity / 100);

    // 已上幽灵的帧号(跳帧显示去重用:与前后帧重叠时不二次叠加)
    QList<int> paintedFrames;

    if (options.skinPrevFrames && frameIndex >= 1)
    {
        // Paint onion skin before current frame.
        qreal prevOpacityIncrement = (maxOpacity - minOpacity) / options.framesToSkinPrev;
        qreal opacity = maxOpacity;

        int onionFrameNumber = layer->getPreviousFrameNumber(frameIndex, options.isAbsolute);
        KeyFrame* currentAbsoluteFrame = layer->getLastKeyFrameAtPosition(frameIndex);
        int currentAbsoluteFrameNumber = currentAbsoluteFrame ? currentAbsoluteFrame->pos() : -1;

        int onionPosition = 0;
        while (onionPosition < options.framesToSkinPrev)
        {
            // We've gone below the first frame, stop iterating
            if (onionFrameNumber < 1) {
                break;
            }
            painter.setOpacity(opacity);

            // When in absolute mode, we don't skin the current absolute frame
            // otherwise, if absolute is off and the current frame is in range, will be painted
            if (!options.isAbsolute || onionFrameNumber != currentAbsoluteFrameNumber) {
                state(OnionSkinPaintState::PREV, onionFrameNumber);
                paintedFrames.append(onionFrameNumber);
                opacity = opacity - prevOpacityIncrement;
                onionPosition++;
            }

            onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber, options.isAbsolute);
        }
    }

    state(OnionSkinPaintState::CURRENT, frameIndex);

    if (options.skinNextFrames)
    {
        // Paint onion skin after current frame.
        qreal nextOpacityIncrement = (maxOpacity - minOpacity) / options.framesToSkinNext;
        qreal opacity = maxOpacity;

        int onionFrameNumber = layer->getNextFrameNumber(frameIndex, options.isAbsolute);
        int onionPosition = 0;

        while (onionPosition < options.framesToSkinNext && onionFrameNumber > 0)
        {
            painter.setOpacity(opacity);
            state(OnionSkinPaintState::NEXT, onionFrameNumber);
            paintedFrames.append(onionFrameNumber);
            opacity = opacity - nextOpacityIncrement;

            onionFrameNumber = layer->getNextFrameNumber(onionFrameNumber, options.isAbsolute);
            onionPosition++;
        }
    }

    // 跳帧显示:任意指定帧额外叠一层幽灵(0=关闭)。
    // 与前后帧同规则:落在当前帧之前按 PREV(红)、之后按 NEXT(蓝)着色;
    // 循环层先回绕到周期内;已画过的帧不重画(避免透明度双重叠加)
    if (options.customFrame > 0)
    {
        const int customDisplay = layer->displayFrameFor(options.customFrame);
        if (customDisplay >= 1 && customDisplay != frameIndex && !paintedFrames.contains(customDisplay))
        {
            painter.setOpacity(maxOpacity);
            state(customDisplay < frameIndex ? OnionSkinPaintState::PREV : OnionSkinPaintState::NEXT,
                  customDisplay);
        }
    }
}
