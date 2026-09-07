# -*- coding: utf-8 -*-
"""Add click-level UI probes so the debug log shows what the user did."""
import io, os

def patch(path, pairs):
    s = io.open(path, encoding='utf-8').read()
    for old, new in pairs:
        assert s.count(old) == 1, (os.path.basename(path), old[:70])
        s = s.replace(old, new)
    io.open(path, 'w', encoding='utf-8', newline='').write(s)
    print('patched', os.path.basename(path))

R = r'C:\Users\zp122\Documents\trae_projects\ceshi\pencil'

# 1) canvas clicks: tool / frame / layer
patch(R + r'\core_lib\src\interface\scribblearea.cpp', [
('void ScribbleArea::pointerPressEvent(PointerEvent* event)\n{',
'''void ScribbleArea::pointerPressEvent(PointerEvent* event)
{
    {
        Layer* l = mEditor->layers()->currentLayer();
        qDebug() << "[ui] canvas press:" << (l ? l->name() : QString("?"))
                 << "frame" << mEditor->currentFrame()
                 << "button" << event->button();
    }'''),
])

# 2) tool switching
patch(R + r'\core_lib\src\managers\toolmanager.cpp', [
('void ToolManager::setCurrentTool(ToolType eToolType)\n{',
'''void ToolManager::setCurrentTool(ToolType eToolType)
{
    qDebug() << "[ui] tool ->" << BaseTool::TypeName(eToolType);'''),
])

# 3) playback start/stop
patch(R + r'\core_lib\src\managers\playbackmanager.cpp', [
('void PlaybackManager::play()\n{',
'''void PlaybackManager::play()
{
    qDebug() << "[ui] playback play";'''),
('void PlaybackManager::stop()\n{',
'''void PlaybackManager::stop()
{
    qDebug() << "[ui] playback stop";'''),
])

# 4) timeline interactions: press intent + release outcome + label toggles
patch(R + r'\app\src\timelinecells.cpp', [
# layer row clicks
('''            if (event->pos().x() < 9)
            {
                // cycle the 8-color label: -1 -> 0 -> ... -> 7 -> -1
                Layer* labelLayer = mEditor->object()->getLayer(layerNumber);
                labelLayer->setColorIndex((labelLayer->colorIndex() + 2) % 9 - 1);
                updateContent();
            }
            else if (event->pos().x() < 30)
            {
                mEditor->switchVisibilityOfLayer(layerNumber);
            }''',
'''            if (event->pos().x() < 9)
            {
                // cycle the 8-color label: -1 -> 0 -> ... -> 7 -> -1
                Layer* labelLayer = mEditor->object()->getLayer(layerNumber);
                labelLayer->setColorIndex((labelLayer->colorIndex() + 2) % 9 - 1);
                qDebug() << "[ui] layer" << layerNumber << "label color ->" << labelLayer->colorIndex();
                updateContent();
            }
            else if (event->pos().x() < 30)
            {
                qDebug() << "[ui] layer" << layerNumber << "toggle visible";
                mEditor->switchVisibilityOfLayer(layerNumber);
            }'''),
# tracks press intent
('''                const int plusLayer = hitTestPlusHandle(event->pos());
                if (plusLayer != -1)
                {''',
'''                const int plusLayer = hitTestPlusHandle(event->pos());
                if (plusLayer != -1)
                {
                    qDebug() << "[ui] trim-drag start: plus handle layer" << plusLayer;'''),
('''                int trimPos = hitTestTrimHandle(event->pos());
                if (trimPos > 0)
                {''',
'''                int trimPos = hitTestTrimHandle(event->pos());
                if (trimPos > 0)
                {
                    qDebug() << "[ui] trim-drag start: block" << trimPos << "layer" << layerNumber;'''),
# release outcomes
('''                trimKey->setLength(mTrimPreviewLength);
                trimKey->setLengthExplicit(true);''',
'''                qDebug() << "[ui] trim-drag end: block" << mTrimKeyPos
                         << "len" << mTrimOriginalLength << "->" << mTrimPreviewLength;
                trimKey->setLength(mTrimPreviewLength);
                trimKey->setLengthExplicit(true);'''),
('''                currentLayer->moveSelectedFrames(offset);
                mEditor->undoRedo()->record(saveStateId, tr("Move Frames"));''',
'''                qDebug() << "[ui] frames moved by" << offset;
                currentLayer->moveSelectedFrames(offset);
                mEditor->undoRedo()->record(saveStateId, tr("Move Frames"));'''),
('''                    for (int i = 0; i < n; i++)
                    {
                        mEditor->scrubTo(startFrame + i);
                        mEditor->addNewKey();
                    }''',
'''                    qDebug() << "[ui] plus-create" << n << "frames from" << startFrame;
                    for (int i = 0; i < n; i++)
                    {
                        mEditor->scrubTo(startFrame + i);
                        mEditor->addNewKey();
                    }'''),
# collapse toggle
('''void TimeLineCells::toggleLayerCollapsed(int layerNumber)
{
    Layer* l = mEditor->object()->getLayer(layerNumber);
    if (l == nullptr) return;''',
'''void TimeLineCells::toggleLayerCollapsed(int layerNumber)
{
    Layer* l = mEditor->object()->getLayer(layerNumber);
    if (l == nullptr) return;
    qDebug() << "[ui] layer" << layerNumber << "collapse toggle";'''),
])
print('ALL DONE')
