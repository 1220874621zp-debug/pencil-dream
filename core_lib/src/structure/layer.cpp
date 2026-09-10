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
#include "layer.h"

#include <QApplication>
#include <QDebug>
#include <QSettings>
#include <QPainter>
#include <QDomElement>
#include <QSet>
#include <algorithm>
#include "keyframe.h"

// Used to sort the selected frames list
bool sortAsc(int left, int right)
{
    return left < right;
}

Layer::Layer(int id, LAYER_TYPE eType)
{
    Q_ASSERT(eType != UNDEFINED);

    meType = eType;
    mName = QString(tr("Undefined Layer"));

    mId = id;
}

Layer::~Layer()
{
    for (auto it : mKeyFrames)
    {
        KeyFrame* pKeyFrame = it.second;
        delete pKeyFrame;
    }
    mKeyFrames.clear();
}

void Layer::foreachKeyFrame(std::function<void(KeyFrame*)> action) const
{
    for (auto pair : mKeyFrames)
    {
        action(pair.second);
    }
}

bool Layer::keyExists(int position) const
{
    return (mKeyFrames.find(position) != mKeyFrames.end());
}

KeyFrame* Layer::getKeyFrameAt(int position) const
{
    auto it = mKeyFrames.find(position);
    if (it == mKeyFrames.end())
    {
        return nullptr;
    }
    return it->second;
}

KeyFrame* Layer::getLastKeyFrameAtPosition(int position) const
{
    if (position < 1)
    {
        position = 1;
    }
    auto it = mKeyFrames.lower_bound(position);
    if (it == mKeyFrames.end())
    {
        return nullptr;
    }
    return it->second;
}

int Layer::getPreviousKeyFramePosition(int position) const
{
    auto it = mKeyFrames.upper_bound(position);
    if (it == mKeyFrames.end())
    {
        return firstKeyFramePosition();
    }
    return it->first;
}

int Layer::getNextKeyFramePosition(int position) const
{
    // workaround: bug with lower_bound?
    // when position is before the first frame it == mKeyFrames.end() for some reason
    if (position < firstKeyFramePosition())
    {
       return firstKeyFramePosition();
    }

    auto it = mKeyFrames.lower_bound(position);
    if (it == mKeyFrames.end())
    {
        return getMaxKeyFramePosition();
    }

    if (it != mKeyFrames.begin())
    {
        --it;
    }
    return it->first;
}

int Layer::getPreviousFrameNumber(int position, bool isAbsolute) const
{
    int prevNumber;

    if (isAbsolute)
        prevNumber = getPreviousKeyFramePosition(position);
    else
        prevNumber = position - 1;

    if (prevNumber >= position)
    {
        return -1; // There is no previous keyframe
    }
    return prevNumber;
}

int Layer::getNextFrameNumber(int position, bool isAbsolute) const
{
    int nextNumber;

    if (isAbsolute)
        nextNumber = getNextKeyFramePosition(position);
    else
        nextNumber = position + 1;

    if (nextNumber <= position)
        return -1; // There is no next keyframe

    return nextNumber;
}

int Layer::firstKeyFramePosition() const
{
    if (!mKeyFrames.empty())
    {
        return mKeyFrames.rbegin()->first; // rbegin is the lowest key frame position
    }
    return 0;
}

int Layer::getMaxKeyFramePosition() const
{
    if (!mKeyFrames.empty())
    {
        return mKeyFrames.begin()->first; // begin is the highest key frame position
    }
    return 0;
}

bool Layer::addNewKeyFrameAt(int position)
{
    if (position <= 0) return false;

    KeyFrame* key = createKeyFrame(position);
    if (!addKeyFrame(position, key))
    {
        delete key;
        return false;
    }
    return true;
}

void Layer::addOrReplaceKeyFrame(int position, KeyFrame* pKeyFrame)
{
    Q_ASSERT(position > 0);
    pKeyFrame->setPos(position);
    loadKey(pKeyFrame);
    markFrameAsDirty(position);
}

bool Layer::addKeyFrame(int position, KeyFrame* pKeyFrame)
{
    if (keyExists(position))
    {
        return false;
    }

    pKeyFrame->setPos(position);
    mKeyFrames.emplace(position, pKeyFrame);
    markFrameAsDirty(position);
    return true;
}

bool Layer::insertExposureAt(int position)
{
    if(position < 1 || position > getMaxKeyFramePosition() || !getKeyFrameAt(position))
    {
        return false;
    }

    newSelectionOfConnectedFrames(position + 1);
    moveSelectedFrames(1);
    deselectAll();

    return true;
}

bool Layer::removeKeyFrame(int position)
{
    if (keyFrameCount() == 1 && this->type() != SOUND) { return false; }
    auto frame = getKeyFrameWhichCovers(position);

    if (frame)
    {
        const int removedPos = frame->pos();
        removeFromSelectionList(removedPos);
        mKeyFrames.erase(removedPos);
        markFrameAsDirty(removedPos);
        delete frame;

        // TVP semantics: the block before the freed span absorbs it so no
        // gap is left behind by a plain delete
        absorbGapAt(removedPos);
    }
    return true;
}

void Layer::removeFromSelectionList(int position)
{
    mSelectedFrames_byLast.removeAll(position);
    mSelectedFrames_byPosition.removeAll(position);
}

bool Layer::moveKeyFrame(int position, int offset)
{
    int newPos = position + offset;
    if (newPos < 1) { return false; }

    auto listOfFramesLast = mSelectedFrames_byLast;
    auto listOfFramesPos = mSelectedFrames_byPosition;
    bool frameSelected = isFrameSelected(position);

    if (swapKeyFrames(position, newPos)) {

        auto oldPosIndex = mSelectedFrames_byPosition.indexOf(position);
        auto newPosIndex = mSelectedFrames_byPosition.indexOf(newPos);

        auto oldLastIndex = mSelectedFrames_byLast.indexOf(position);
        auto newLastndex = mSelectedFrames_byLast.indexOf(newPos);


        // Old position is selected
        if (oldPosIndex != -1) {
            mSelectedFrames_byPosition[oldPosIndex] = newPos;
            mSelectedFrames_byLast[oldLastIndex] = newPos;
        }

        // Old position is selected
        if (newPosIndex != -1) {
            mSelectedFrames_byPosition[newPosIndex] = position;
            mSelectedFrames_byLast[newLastndex] = position;
        }
        return true;
    }

    mSelectedFrames_byLast.clear();
    mSelectedFrames_byPosition.clear();

    setFrameSelected(position, true);

    // If the move fails, assume we can't move at all and revert to old selection
    if (!moveSelectedFrames(offset)) {
        mSelectedFrames_byLast = listOfFramesLast;
        mSelectedFrames_byPosition = listOfFramesPos;
        return false;
    }

    // Remove old position from selection list
    listOfFramesLast.removeOne(position);
    listOfFramesPos.removeOne(position);

    mSelectedFrames_byLast = listOfFramesLast;
    mSelectedFrames_byPosition = listOfFramesPos;

    // If the frame was selected prior to moving, make sure it's still selected.
    setFrameSelected(newPos, frameSelected);

    return true;
}

// Current behaviour, need to refresh the swapped cels
bool Layer::swapKeyFrames(int position1, int position2)
{
    KeyFrame* pFirstFrame = nullptr;
    KeyFrame* pSecondFrame = nullptr;

    if (mKeyFrames.count(position1) != 1 || mKeyFrames.count(position2) != 1)
    {
        return false;
    }

    // Both keys exist
    pFirstFrame = mKeyFrames[position1];
    pSecondFrame = mKeyFrames[position2];

    mKeyFrames[position1] = pSecondFrame;
    mKeyFrames[position2] = pFirstFrame;

    pFirstFrame->setPos(position2);
    pSecondFrame->setPos(position1);

    pFirstFrame->modification();
    pSecondFrame->modification();

    markFrameAsDirty(position1);
    markFrameAsDirty(position2);

    return true;
}

bool Layer::loadKey(KeyFrame* pKey)
{
    auto it = mKeyFrames.find(pKey->pos());
    if (it != mKeyFrames.end())
    {
        delete it->second;
        mKeyFrames.erase(it);
    }
    mKeyFrames.emplace(pKey->pos(), pKey);
    return true;
}

Status Layer::save(const QString& sDataFolder, QStringList& attachedFiles, ProgressCallback progressStep)
{
    DebugDetails dd;
    dd << "\n[Layer SAVE diagnostics]\n";

    bool ok = true;

    for (auto pair : mKeyFrames)
    {
        KeyFrame* keyFrame = pair.second;
        Status st = saveKeyFrameFile(keyFrame, sDataFolder);
        if (st.ok())
        {
            //qDebug() << "Layer [" << name() << "] FN=" << keyFrame->fileName();
            if (!keyFrame->fileName().isEmpty())
                attachedFiles.append(keyFrame->fileName());
        }
        else
        {
            ok = false;
            dd.collect(st.details());
            dd << QString("- Keyframe[%1] failed to save").arg(keyFrame->pos());
        }
        progressStep();
    }
    if (!ok)
    {
        dd << "\nError: Failed to save one or more files";
        return Status(Status::FAIL, dd);
    }
    return Status::OK;
}

void Layer::setModified(int position, bool modified) const
{
    KeyFrame* key = getKeyFrameAt(position);
    if (key)
    {
        key->setModified(modified);
    }
}

bool Layer::isFrameSelected(int position) const
{
    KeyFrame* keyFrame = getKeyFrameWhichCovers(position);
    if (keyFrame == nullptr) { return false; }

    return mSelectedFrames_byLast.contains(keyFrame->pos());
}

void Layer::setFrameSelected(int position, bool isSelected)
{
    KeyFrame* keyFrame = getKeyFrameWhichCovers(position);
    if (keyFrame != nullptr)
    {
        int startPosition = keyFrame->pos();

        if (isSelected && !mSelectedFrames_byLast.contains(startPosition))
        {
            // Add the selected frame to the lists
            mSelectedFrames_byLast.insert(0, startPosition);
            mSelectedFrames_byPosition.append(startPosition);

            // We need to keep the list of selected frames sorted
            // in order to easily handle their movement
            std::sort(mSelectedFrames_byPosition.begin(), mSelectedFrames_byPosition.end(), sortAsc);
        }
        else if (!isSelected)
        {
            mSelectedFrames_byLast.removeOne(startPosition);
            mSelectedFrames_byPosition.removeOne(startPosition);
        }
    }
}

void Layer::toggleFrameSelected(int position, bool allowMultiple)
{
    bool wasSelected = isFrameSelected(position);

    if (!allowMultiple)
    {
        deselectAll();
    }

    setFrameSelected(position, !wasSelected);
}

void Layer::extendSelectionTo(int position)
{
    if (mSelectedFrames_byLast.count() > 0)
    {
        int lastSelected = mSelectedFrames_byLast[0];
        int startPos;
        int endPos;

        if (lastSelected < position)
        {
            startPos = lastSelected;
            endPos = position;
        }
        else
        {
            startPos = position;
            endPos = lastSelected;
        }

        // 按块跳跃而非逐帧：块内所有帧都解析到同一关键帧，逐帧循环在大跨度
        // （Alt/Shift 选中数千帧）时是 O(跨度 × 已选数)，块跳跃是 O(块数 log)
        int i = startPos;
        while (i <= endPos)
        {
            KeyFrame* key = getKeyFrameWhichCovers(i);
            if (key == nullptr) { ++i; continue; }
            setFrameSelected(i, true);
            const int blockEnd = getBlockEnd(key);
            if (blockEnd < 0) { break; } // 开放式末块：后面没有别的键了
            i = (blockEnd > i) ? blockEnd : (i + 1);
        }
    }
}

bool Layer::newSelectionOfConnectedFrames(int position)
{
    // Deselect all before extending to make sure we don't get already
    // selected frames
    deselectAll();

    if (!keyExists(position)) { return false; }

    setFrameSelected(position, true);

    // Find keyframes that are connected and make sure we're below max.
    while (position < getMaxKeyFramePosition() && getKeyFrameWhichCovers(position) != nullptr) {
        position++;
    }

    extendSelectionTo(position);

    return true;
}

void Layer::selectAllFramesAfter(int position)
{
    int startPosition = position;
    int endPosition = getMaxKeyFramePosition();

    if (!keyExists(startPosition))
    {
        startPosition = getNextKeyFramePosition(startPosition);
    }

    if (startPosition > 0 && startPosition <= endPosition)
    {
        deselectAll();
        setFrameSelected(startPosition, true);
        extendSelectionTo(endPosition);
    }
}

void Layer::deselectAll()
{
    mSelectedFrames_byLast.clear();
    mSelectedFrames_byPosition.clear();
}

bool Layer::canMoveSelectedFramesToOffset(int offset) const
{
    // 目标位置一次性入 QSet：原来对 QList 的 contains 内层循环是 O(n²)，
    // moveSelectedFrames 的位移递增重试还会把它再放大一个量级
    QSet<int> targetSet;
    targetSet.reserve(mSelectedFrames_byPosition.count());
    for (int pos : mSelectedFrames_byPosition)
    {
        targetSet.insert(pos + offset);
    }

    for (int pos : mSelectedFrames_byPosition)
    {
        const int newPos = pos + offset;
        // 目标位被占用且占用者不在本次移动集合内（不会被腾出来）→ 不可移
        if (keyExists(newPos) && !targetSet.contains(newPos)) {
            return false;
        }
    }

    return true;
}

void Layer::setExposureForSelectedFrames(int offset)
{
    auto selectedFramesByLast = mSelectedFrames_byLast;
    auto selectedFramesByPos = mSelectedFrames_byPosition;

    int addSpaceBetweenFrames = offset;

    if (selectedFramesByLast.isEmpty()) { return; }

    const int max = selectedFramesByPos.count()-1;
    const int posForIndex = selectedFramesByPos[max];
    const int nextPos = getNextKeyFramePosition(selectedFramesByPos[max]);

    // When exposing the frame in front of the right most element in the selection.
    if (posForIndex != nextPos) {
        selectedFramesByPos.append(nextPos);
    }

    auto initialLastList = selectedFramesByLast;
    auto initialPosList = selectedFramesByPos;

    auto offsetList = QList<int>();

    // Create an offset list to have reference of how many frames should be moved
    for (int offset = 0; offset < selectedFramesByPos.count(); offset++)
    {
        int pos = selectedFramesByPos[offset];
        int nextKeyPos = getNextKeyFramePosition(pos);

        if (pos >= getMaxKeyFramePosition()) {
            offsetList << (pos - 1) - getPreviousKeyFramePosition(pos) + addSpaceBetweenFrames;
        } else { // first frame doesn't move so only the space that's required
            offsetList << nextKeyPos - (pos + 1) + addSpaceBetweenFrames;
        }
    }

    // Either positive or negative
    int offsetDirection = offset > 0 ? 1 : -1;

    for (int i = 0; i < selectedFramesByPos.count(); i++) {
        const int itPos = selectedFramesByPos[i];
        const int nextIndex = i + 1;
        const int positionInFront = itPos + 1;

        // Index safety
        if (nextIndex < 0 || nextIndex >= selectedFramesByPos.count()) {
            continue;
        }

        // Offset above 0 will move frames forward
        // Offset below 0 will move a frame backwards
        while ((offset > 0 && getNextKeyFramePosition(itPos) - positionInFront < offsetList[i]) ||
               (getNextKeyFramePosition(itPos) - positionInFront > offsetList[i] && getNextKeyFramePosition(itPos) - positionInFront > 0)) {

           selectAllFramesAfter(getNextKeyFramePosition(itPos));

           for (int selIndex = 0; selIndex < mSelectedFrames_byPosition.count(); selIndex++) {

               if (nextIndex+selIndex >= selectedFramesByPos.count()) { break; }

               int pos = selectedFramesByPos[nextIndex+selIndex];

               if (!mSelectedFrames_byPosition.contains(pos)) { continue; }

               selectedFramesByPos[nextIndex+selIndex] = pos + offsetDirection;

               // To make the sure we get the correct index for last selection list
               // use the initial list where values doesn't affect the index.
               int initialPos = initialPosList[nextIndex+selIndex];
               int indexOfLast = initialLastList.indexOf(initialPos);
               if (indexOfLast == -1 || nextIndex+selIndex >= selectedFramesByLast.count()) {
                   continue;
               }
               selectedFramesByLast[indexOfLast] = selectedFramesByLast[indexOfLast] + offsetDirection;
           }

           moveSelectedFrames(offsetDirection);
        }
    }

    deselectAll();

    // Reselect frames again based on last selection list to ensure selection behaviour
    // works correctly
    for (int pos : selectedFramesByLast) {
        Q_UNUSED(pos)
        setFrameSelected(selectedFramesByLast.takeLast(), true);
    }
}

bool Layer::reverseOrderOfSelection()
{
    QList<int> selectedIndexes = mSelectedFrames_byPosition;

    if (selectedIndexes.isEmpty()) { return false; }

    for (int swapBegin = 0, swapEnd = selectedIndexes.count()-1; swapBegin < swapEnd; swapBegin++, swapEnd--) {
        int oldPos = selectedIndexes[swapBegin];
        int newPos = selectedIndexes[swapEnd];
        bool canSwap = swapKeyFrames(oldPos, newPos);
        Q_ASSERT(canSwap);
    }
    return true;
}

bool Layer::moveSelectedFrames(int offset)
{
    if (offset == 0 || mSelectedFrames_byPosition.count() <= 0) {
        return false;
    }

    // If we are moving to the right we start moving selected frames from the highest (right) to the lowest (left)
    int indexInSelection = mSelectedFrames_byPosition.count() - 1;
    int step = -1;

    if (offset < 0)
    {
        // If we are moving to the left we start moving selected frames from the lowest (left) to the highest (right)
        indexInSelection = 0;
        step = 1;

        // Check if we are not moving out of the timeline
        if (mSelectedFrames_byPosition[0] + offset < 1) {
            offset = 1 - mSelectedFrames_byPosition[0];
        }
    }

    while (!canMoveSelectedFramesToOffset(offset)) { offset += 1; }
    if (offset == 0) { return false; }

    for (; indexInSelection > -1 && indexInSelection < mSelectedFrames_byPosition.count(); indexInSelection += step)
    {
        int fromPos = mSelectedFrames_byPosition[indexInSelection];
        int toPos = fromPos + offset;

        // Get the frame to move
        KeyFrame* selectedFrame = getKeyFrameAt(fromPos);

        Q_ASSERT(!keyExists(toPos));

        mKeyFrames.erase(fromPos);
        markFrameAsDirty(fromPos);

        // Update the position of the selected frame
        selectedFrame->setPos(toPos);
        mKeyFrames.insert(std::make_pair(toPos, selectedFrame));
        markFrameAsDirty(toPos);
    }

    // Update selection lists
    for (int& pos : mSelectedFrames_byPosition)
    {
        pos += offset;
    }
    for (int& pos : mSelectedFrames_byLast)
    {
        pos += offset;
    }
    return true;
}

bool Layer::isPaintable() const
{
    return (type() == BITMAP || type() == VECTOR || type() == COLORIZE);
}

bool Layer::keyExistsWhichCovers(int frameNumber)
{
    return getKeyFrameWhichCovers(frameNumber) != nullptr;
}

KeyFrame* Layer::getKeyFrameWhichCovers(int frameNumber) const
{
    // 字面覆盖查找：时间轴几何/命中测试/碰撞检测/声音调度依赖它，
    // 不做循环重映射（循环层取帧显示走 displayFrameFor 显式重映射）
    auto keyFrame = getLastKeyFrameAtPosition(frameNumber);
    if (keyFrame != nullptr)
    {
        // Auto-length frames hold until the next keyframe; explicit ones stop
        // after their length. Guard against zero/negative explicit lengths
        // (treated as auto) so a corrupted length can never blank drawing.
        const int len = keyFrame->length();
        const int cover = (keyFrame->isLengthExplicit() && len > 0) ? len : INT_MAX;
        // qint64: pos + INT_MAX overflows int and flips negative, which made
        // every auto-length keyframe miss and blanked the canvas
        if (static_cast<qint64>(keyFrame->pos()) + cover > frameNumber)
        {
            return keyFrame;
        }
    }
    return nullptr;
}

int Layer::displayFrameFor(int frameNumber) const
{
    if (mLoopMode == LoopMode::None || !isBitmapKind() || mKeyFrames.empty())
    {
        return frameNumber;
    }

    // mKeyFrames 降序：begin() = 末键（最大 pos），rbegin() = 首键
    const int start = mKeyFrames.rbegin()->first;
    const KeyFrame* lastKey = mKeyFrames.begin()->second;
    // 显式尾块（trim过）= 播完即无，永不回绕
    if (lastKey->isLengthExplicit())
    {
        return frameNumber;
    }
    // 开放尾块内容记 1 帧
    const int cycleEnd = lastKey->pos() + 1;
    const int cycleLen = cycleEnd - start;
    if (cycleLen <= 1 || frameNumber < cycleEnd)
    {
        return frameNumber;
    }

    const int offset = frameNumber - start;
    if (mLoopMode == LoopMode::Cycle)
    {
        return start + offset % cycleLen;
    }

    // 往复：周期 2N-2 的三角波（首尾帧不重复出现）
    const int period = 2 * cycleLen - 2;
    int index = offset % period;
    if (index >= cycleLen)
    {
        index = period - index;
    }
    return start + index;
}

int Layer::getBlockEnd(const KeyFrame* key) const
{
    if (key == nullptr) { return -1; }

    // mKeyFrames is sorted descending (greater<int>): larger positions sit towards begin()
    auto it = mKeyFrames.find(key->pos());
    if (it == mKeyFrames.end()) { return -1; }

    bool hasNextKeyframe = (it != mKeyFrames.begin());
    int nextKeyframePos = -1;
    if (hasNextKeyframe)
    {
        auto nextIt = it;
        --nextIt; // the following keyframe has a larger position
        nextKeyframePos = nextIt->first;
    }

    if (key->isLengthExplicit())
    {
        int end = key->pos() + key->length();
        if (hasNextKeyframe && nextKeyframePos < end)
        {
            end = nextKeyframePos;
        }
        return end; // exclusive end
    }

    // Auto length: the block holds until the next keyframe, if any
    return hasNextKeyframe ? nextKeyframePos : -1;
}

KeyFrame* Layer::takeKeyFrame(int position)
{
    auto it = mKeyFrames.find(position);
    if (it == mKeyFrames.end())
    {
        return nullptr;
    }
    KeyFrame* key = it->second;
    mKeyFrames.erase(it);
    removeFromSelectionList(position);
    markFrameAsDirty(position);
    return key;
}

bool Layer::containsKeyFramePointer(const KeyFrame* key) const
{
    for (const auto& pair : mKeyFrames)
    {
        if (pair.second == key) { return true; }
    }
    return false;
}

KeyFrame* Layer::takeKeyFrameByPointer(KeyFrame* key)
{
    for (auto it = mKeyFrames.begin(); it != mKeyFrames.end(); ++it)
    {
        if (it->second == key)
        {
            const int pos = it->first;
            mKeyFrames.erase(it);
            removeFromSelectionList(pos);
            markFrameAsDirty(pos);
            return key;
        }
    }
    return nullptr;
}

void Layer::repositionKeyFrame(KeyFrame* key, int newPos)
{
    Q_ASSERT(key != nullptr && newPos >= 1);
    for (auto it = mKeyFrames.begin(); it != mKeyFrames.end(); ++it)
    {
        if (it->second == key)
        {
            const int oldPos = it->first;
            mKeyFrames.erase(it);
            key->setPos(newPos);
            mKeyFrames.emplace(newPos, key);
            markFrameAsDirty(oldPos);
            markFrameAsDirty(newPos);
            return;
        }
    }
    Q_ASSERT_X(false, "Layer::repositionKeyFrame", "keyframe pointer not owned by this layer");
}

QList<KeyFrameLayoutEntry> Layer::captureKeyFrameLayout() const
{
    QList<KeyFrameLayoutEntry> layout;
    // walk from the lowest position so snapshots apply in ascending order
    for (auto it = mKeyFrames.rbegin(); it != mKeyFrames.rend(); ++it)
    {
        KeyFrameLayoutEntry entry;
        entry.key = it->second;
        entry.pos = it->first;
        entry.length = it->second->length();
        entry.lengthExplicit = it->second->isLengthExplicit();
        layout.append(entry);
    }
    return layout;
}

void Layer::applyKeyFrameLayout(const QList<KeyFrameLayoutEntry>& layout, QList<KeyFrame*>& extracted)
{
    extracted.clear();

    QSet<KeyFrame*> listed;
    for (const KeyFrameLayoutEntry& entry : layout)
    {
        if (entry.key != nullptr) { listed.insert(entry.key); }
    }

    // keys currently in the layer but absent from the layout leave the layer
    QList<KeyFrame*> current;
    QList<int> oldPositions;
    for (const auto& pair : mKeyFrames)
    {
        current.append(pair.second);
        oldPositions.append(pair.first);
    }
    for (KeyFrame* key : current)
    {
        if (!listed.contains(key))
        {
            extracted.append(takeKeyFrameByPointer(key));
        }
    }

    // wholesale rebuild: no map slot can be claimed twice by a moving key
    mKeyFrames.clear();
    for (const KeyFrameLayoutEntry& entry : layout)
    {
        if (entry.key == nullptr) { continue; }
        entry.key->setPos(entry.pos);
        entry.key->setLength(entry.length);
        entry.key->setLengthExplicit(entry.lengthExplicit);
        const auto result = mKeyFrames.emplace(entry.pos, entry.key);
        Q_ASSERT_X(result.second, "Layer::applyKeyFrameLayout", "duplicate position in layout");
        if (!result.second)
        {
            extracted.append(entry.key); // refuse to host: hand back to the caller
        }
    }

    for (int pos : oldPositions) { markFrameAsDirty(pos); }
    for (const KeyFrameLayoutEntry& entry : layout) { markFrameAsDirty(entry.pos); }
    deselectAll();
}

void Layer::absorbGapAt(int fromPos)
{
    if (fromPos < 1) { return; }

    const int prevPos = getPreviousKeyFramePosition(fromPos);
    if (prevPos < 1) { return; }
    KeyFrame* prevKey = getKeyFrameAt(prevPos);
    if (prevKey == nullptr) { return; }

    // auto-length blocks already hold until the next keyframe: nothing to do
    if (!prevKey->isLengthExplicit()) { return; }

    const int nextPos = getNextKeyFramePosition(fromPos);
    if (nextPos <= prevPos) { return; } // nothing after the gap; trailing block stays

    const int newLen = nextPos - prevPos;
    if (newLen != prevKey->length())
    {
        prevKey->setLength(newLen);
        prevKey->modification();
        markFrameAsDirty(prevPos);
    }
}

void Layer::absorbGapsAt(const QList<int>& positions)
{
    QList<int> sorted = positions;
    std::sort(sorted.begin(), sorted.end());
    for (int pos : sorted)
    {
        if (!keyExists(pos))
        {
            absorbGapAt(pos);
        }
    }
}

QDomElement Layer::createBaseDomElement(QDomDocument& doc) const
{
    QDomElement layerTag = doc.createElement("layer");
    layerTag.setAttribute("id", id());
    layerTag.setAttribute("name", name());
    layerTag.setAttribute("visibility", visible());
    layerTag.setAttribute("type", type());
    if (mOpacity < 1.0)
    {
        layerTag.setAttribute("opacity", QString::number(mOpacity, 'f', 3));
    }
    if (mLocked)
    {
        layerTag.setAttribute("locked", 1);
    }
    if (mClipMask)
    {
        layerTag.setAttribute("clipMask", 1);
    }
    if (mColorIndex >= 0)
    {
        layerTag.setAttribute("colorIndex", mColorIndex);
    }
    if (mGroupId >= 0)
    {
        layerTag.setAttribute("group", mGroupId);
    }
    if (mLoopMode != LoopMode::None)
    {
        layerTag.setAttribute("loopMode", static_cast<int>(mLoopMode));
    }
    return layerTag;
}

void Layer::loadBaseDomElement(const QDomElement& elem)
{
    if (!elem.attribute("id").isNull())
    {
        int id = elem.attribute("id").toInt();
        setId(id);
    }
    setName(elem.attribute("name", "untitled"));
    setVisible(elem.attribute("visibility", "1").toInt());
    setOpacity(elem.attribute("opacity", "1").toDouble());
    setLocked(elem.attribute("locked", "0").toInt() == 1);
    setClipMask(elem.attribute("clipMask", "0").toInt() == 1);
    mColorIndex = elem.attribute("colorIndex", "-1").toInt();
    mGroupId = elem.attribute("group", "-1").toInt();
    mLoopMode = static_cast<LoopMode>(qBound(0, elem.attribute("loopMode", "0").toInt(), 2));
}
