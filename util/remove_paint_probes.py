# -*- coding: utf-8 -*-
"""Remove the temporary paint-diagnosis probes (keep the log clean)."""
import io, os

R = r'C:\Users\zp122\Documents\trae_projects\ceshi\pencil'

# layer.cpp: covers-miss log
p = R + r'\core_lib\src\structure\layer.cpp'
s = io.open(p, encoding='utf-8').read()
old = '''        qDebug() << "[covers] miss: layer-key pos=" << keyFrame->pos()
                 << " len=" << len
                 << " explicit=" << keyFrame->isLengthExplicit()
                 << " frame=" << frameNumber;
'''
assert old in s, 'layer probe not found'
s = s.replace(old, '')
io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('layer.cpp cleaned')

# canvaspainter.cpp
p = R + r'\core_lib\src\canvaspainter.cpp'
s = io.open(p, encoding='utf-8').read()
old = '''
    if (paintedImage == nullptr)
        qDebug() << "[paint] render: no covering key at frame" << mFrameNumber << "- nothing drawn";'''
assert old in s, 'canvaspainter probe not found'
s = s.replace(old, '')
io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('canvaspainter.cpp cleaned')

# scribblearea.cpp: five probe blocks
p = R + r'\core_lib\src\interface\scribblearea.cpp'
s = io.open(p, encoding='utf-8').read()
blocks = [
'''    {
        Layer* l = mEditor->layers()->currentLayer();
        const int f = mEditor->currentFrame();
        qDebug() << "[paint] press tool=" << int(currentTool()->type())
                 << " frame=" << f
                 << " layer=" << (l ? l->name() : QString("null"))
                 << " keyAt=" << (l ? (l->getKeyFrameAt(f) != nullptr) : false)
                 << " covers=" << (l ? (l->getKeyFrameWhichCovers(f) != nullptr) : false)
                 << " last=" << (l ? (l->getLastKeyFrameAtPosition(f) != nullptr) : false);
    }
''',
'''        qDebug() << "[paint] paintBitmapBuffer: no covering key at" << frameNumber << "- DROPPED";
''',
'''    qDebug() << "[paint] paintBitmapBuffer: frame=" << frameNumber
             << " target=" << (currentBitmapImage(layer) != nullptr);
''',
'''    qDebug() << "[paint] emptyFrame action=" << action
             << " previousKey=" << (previousKeyFrame != nullptr);
''',
]
for b in blocks:
    assert b in s, b[:60]
    s = s.replace(b, '', 1)
io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('scribblearea.cpp cleaned')

# stroketool.cpp
p = R + r'\core_lib\src\tool\stroketool.cpp'
s = io.open(p, encoding='utf-8').read()
old = '''        qDebug() << "[paint] startStroke: handling empty frame first";
'''
assert old in s, 'stroketool probe not found'
s = s.replace(old, '', 1)
io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('stroketool.cpp cleaned')
