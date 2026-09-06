# -*- coding: utf-8 -*-
import io, os

def patch(path, pairs):
    s = io.open(path, encoding='utf-8').read()
    for old, new in pairs:
        assert s.count(old) == 1, (path, old[:60])
        s = s.replace(old, new)
    io.open(path, 'w', encoding='utf-8', newline='').write(s)
    print('patched', os.path.basename(path))

R = r'C:\Users\zp122\Documents\trae_projects\ceshi\pencil'

patch(R + r'\core_lib\src\interface\scribblearea.cpp', [
(
'''void ScribbleArea::pointerPressEvent(PointerEvent* event)
{''',
'''void ScribbleArea::pointerPressEvent(PointerEvent* event)
{
    {
        Layer* l = mEditor->layers()->currentLayer();
        const int f = mEditor->currentFrame();
        qDebug() << "[paint] press tool=" << int(currentTool()->type())
                 << " frame=" << f
                 << " layer=" << (l ? l->name() : QString("null"))
                 << " keyAt=" << (l ? (l->getKeyFrameAt(f) != nullptr) : false)
                 << " covers=" << (l ? (l->getKeyFrameWhichCovers(f) != nullptr) : false)
                 << " last=" << (l ? (l->getLastKeyFrameAtPosition(f) != nullptr) : false);
    }'''
),
(
'''    // If there is no keyframe at or before the current position,
    // just return (since we have nothing to paint on).
    if (layer->getKeyFrameWhichCovers(frameNumber) == nullptr)
    {
        updateFrame();
        return;
    }''',
'''    // If there is no keyframe at or before the current position,
    // just return (since we have nothing to paint on).
    if (layer->getKeyFrameWhichCovers(frameNumber) == nullptr)
    {
        qDebug() << "[paint] paintBitmapBuffer: no covering key at" << frameNumber << "- DROPPED";
        updateFrame();
        return;
    }
    qDebug() << "[paint] paintBitmapBuffer: frame=" << frameNumber
             << " target=" << (currentBitmapImage(layer) != nullptr);'''
),
(
'''    // Drawing on an empty frame; take action based on preference.
    int action = mPrefs->getInt(SETTING::DRAW_ON_EMPTY_FRAME_ACTION);
    auto previousKeyFrame = layer->getKeyFrameWhichCovers(frameNumber);''',
'''    // Drawing on an empty frame; take action based on preference.
    int action = mPrefs->getInt(SETTING::DRAW_ON_EMPTY_FRAME_ACTION);
    auto previousKeyFrame = layer->getKeyFrameWhichCovers(frameNumber);
    qDebug() << "[paint] emptyFrame action=" << action
             << " previousKey=" << (previousKeyFrame != nullptr);'''
),
])

patch(R + r'\core_lib\src\tool\stroketool.cpp', [
(
'''    if (emptyFrameActionEnabled())
    {
        mScribbleArea->handleDrawingOnEmptyFrame();
    }''',
'''    if (emptyFrameActionEnabled())
    {
        qDebug() << "[paint] startStroke: handling empty frame first";
        mScribbleArea->handleDrawingOnEmptyFrame();
    }'''
),
])

# canvaspainter: log when paintedImage is null
p = R + r'\core_lib\src\canvaspainter.cpp'
s = io.open(p, encoding='utf-8').read()
i = s.find('BitmapImage* paintedImage = static_cast<BitmapImage*>(bitmapLayer->getKeyFrameWhichCovers(mFrameNumber));')
assert i > 0
anchor = 'BitmapImage* paintedImage = static_cast<BitmapImage*>(bitmapLayer->getKeyFrameWhichCovers(mFrameNumber));'
inject = anchor + '''
    if (paintedImage == nullptr)
        qDebug() << "[paint] render: no covering key at frame" << mFrameNumber << "- nothing drawn";'''
assert s.count(anchor) == 1
s = s.replace(anchor, inject)
io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('patched canvaspainter.cpp')
