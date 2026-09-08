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
#ifndef LAYER_H
#define LAYER_H

#include <map>
#include <functional>
#include <QObject>
#include <QString>
#include <QDomElement>
#include "pencilerror.h"

class KeyFrame;
class Status;

typedef std::function<void()> ProgressCallback;

/** One keyframe's placement inside a layer, used by layout snapshots
 *  (TVP batch operations: hold-N, batch delete, paste, move, trim ripple). */
struct KeyFrameLayoutEntry
{
    KeyFrame* key = nullptr;
    int pos = 0;
    int length = 1;
    bool lengthExplicit = false;
};

class Layer
{
    Q_DECLARE_TR_FUNCTIONS(Layer)
public:
    enum LAYER_TYPE
    {
        UNDEFINED = 0,
        BITMAP = 1,
        VECTOR = 2,
        MOVIE = 3, // not supported yet
        SOUND = 4,
        CAMERA = 5,
        COLORIZE = 6, // 智能填色图层（继承位图行为）
    };

    explicit Layer(int id, LAYER_TYPE eType);
    virtual ~Layer();

    int id() const { return mId; }
    void setId(int layerId) { mId = layerId; }
    LAYER_TYPE type() const { return meType; }

    /** 位图族图层（位图/智能填色）：共享帧操作与位图编辑管线 */
    bool isBitmapKind() const { return meType == BITMAP || meType == COLORIZE; }

    void setName(QString name) { mName = name; }
    QString name() const { return mName; }

    /** Color label index of the layer (-1 = none, 0-7 = preset label colors) */
    int colorIndex() const { return mColorIndex; }
    void setColorIndex(int colorIndex) { mColorIndex = colorIndex; }

    void switchVisibility() { mVisible = !mVisible; }

    bool visible() const { return mVisible; }
    void setVisible(bool b) { mVisible = b; }

    /** Layer opacity in the 0..1 range (TVP layer-panel slider; multiplies
     *  the per-frame image opacity when the layer is composited). */
    qreal opacity() const { return mOpacity; }
    void setOpacity(qreal opacity) { mOpacity = qBound(0.0, opacity, 1.0); }

    /** A locked layer rejects painting and timeline frame edits (TVP padlock). */
    bool locked() const { return mLocked; }
    void setLocked(bool b) { mLocked = b; }

    /** Clipping mask (PS-style): the layer is clipped to the alpha of the
     *  accumulated layers below it. Consecutive clipped layers share the same
     *  base (the first non-clipped layer below the run). Bitmap layers only. */
    bool clipMask() const { return mClipMask; }
    void setClipMask(bool b) { mClipMask = b; }

    /** Get selected keyframe positions sorted by position */
    QList<int> selectedKeyFramesPositions() const { return mSelectedFrames_byPosition; }

    /** Get selected keyframe positions based on the order they were selected */
    QList<int> selectedKeyFramesByLast() const { return mSelectedFrames_byLast; }

    virtual Status saveKeyFrameFile(KeyFrame*, QString dataPath) = 0;
    virtual void loadDomElement(const QDomElement& element, QString dataDirPath, ProgressCallback progressForward) = 0;
    virtual QDomElement createDomElement(QDomDocument& doc) const = 0;
    QDomElement createBaseDomElement(QDomDocument& doc) const;
    void loadBaseDomElement(const QDomElement& elem);

    // KeyFrame interface
    int getMaxKeyFramePosition() const;
    int firstKeyFramePosition() const;

    bool keyExists(int position) const;
    int  getPreviousKeyFramePosition(int position) const;
    int  getNextKeyFramePosition(int position) const;
    int  getPreviousFrameNumber(int position, bool isAbsolute) const;
    int  getNextFrameNumber(int position, bool isAbsolute) const;

    int keyFrameCount() const { return static_cast<int>(mKeyFrames.size()); }
    int selectedKeyFrameCount() const { return mSelectedFrames_byPosition.count(); }
    bool hasAnySelectedFrames() const { return !mSelectedFrames_byLast.empty() && !mSelectedFrames_byPosition.empty(); }

    /** Will insert an empty frame (exposure) after the given position
        @param position The frame to add exposure to
    */
    bool insertExposureAt(int position);

    /**
     * Creates a new keyframe at the given position, unless one already exists.
     * @param position The position of the new keyframe
     * @return false if a keyframe already exists at the position, true if the new keyframe was successfully added
     */
    bool addNewKeyFrameAt(int position);
    void addOrReplaceKeyFrame(int position, KeyFrame* pKeyFrame);
    /**
     * Adds a keyframe at the given position, unless one already exists.
     * @param position The new position of the keyframe
     * @param pKeyFrame The keyframe to add. Its previous position will be overwritten
     * @return false if a keyframe already exists at the position, true if the keyframe was successfully added
     */
    virtual bool addKeyFrame(int position, KeyFrame* pKeyFrame);
    virtual bool removeKeyFrame(int position);
    virtual void replaceKeyFrame(const KeyFrame* pKeyFrame) = 0;

    bool swapKeyFrames(int position1, int position2);
    bool moveKeyFrame(int position, int offset);
    KeyFrame* getKeyFrameAt(int position) const;
    KeyFrame* getLastKeyFrameAtPosition(int position) const;
    bool keyExistsWhichCovers(int frameNumber);
    KeyFrame *getKeyFrameWhichCovers(int frameNumber) const;

    /** Returns the exclusive end position of the exposure block of the given keyframe.
     *  Auto-length frames hold until the next keyframe; explicit (trimmed) frames stop
     *  after their length. Returns -1 when the block is open-ended (last keyframe, auto length). */
    int getBlockEnd(const KeyFrame* key) const;

    /** Removes the keyframe at the given position and transfers ownership to the caller
     *  (unlike removeKeyFrame, the frame is not deleted and the last-frame restriction does not apply). */
    KeyFrame* takeKeyFrame(int position);

    // --- TVP layout-transaction support -----------------------------------
    /** Whether the given keyframe pointer currently lives in this layer. */
    bool containsKeyFramePointer(const KeyFrame* key) const;

    /** Removes the keyframe identified by pointer (ownership passes to the caller). */
    KeyFrame* takeKeyFrameByPointer(KeyFrame* key);

    /** Moves a keyframe already owned by the layer to a new position
     *  (raw map move, no selection bookkeeping). */
    void repositionKeyFrame(KeyFrame* key, int newPos);

    /** Captures the full keyframe layout (pointer, position, length, explicit flag). */
    QList<KeyFrameLayoutEntry> captureKeyFrameLayout() const;

    /** Restores a captured layout: keys listed but currently absent must be owned
     *  by the caller and are inserted (ownership passes to the layer); keys currently
     *  in the layer but absent from the layout are extracted into `extracted`
     *  (ownership passes to the caller). */
    void applyKeyFrameLayout(const QList<KeyFrameLayoutEntry>& layout, QList<KeyFrame*>& extracted);

    /** TVP-style gap absorption: the explicit block ending before `fromPos`
     *  extends to the next keyframe so the freed span is covered.
     *  Auto-length blocks absorb implicitly, so this is a no-op for them. */
    void absorbGapAt(int fromPos);

    /** Absorbs gaps left behind at every given position (skip positions that
     *  still hold a keyframe). */
    void absorbGapsAt(const QList<int>& positions);

    void foreachKeyFrame(std::function<void(KeyFrame*)>) const;

    void setModified(int position, bool isModified) const;

    // Handle selection
    bool isFrameSelected(int position) const;
    void setFrameSelected(int position, bool isSelected);
    void toggleFrameSelected(int position, bool allowMultiple = false);
    void extendSelectionTo(int position);
    void selectAllFramesAfter(int position);

    /** Make a selection from specified position until a blank spot appears
     *  The search is only looking forward, e.g.
     *  @code
     *  |123| 4 5
     *   ^
     *   pos/search from
     *  @endcode
     *  @param position the current position
     */
    bool newSelectionOfConnectedFrames(int position);

    /** Add or subtract exposure from selected frames
     * @param offset Any value above 0 for adding exposure and any value below 0 to subtract exposure
     */
    void setExposureForSelectedFrames(int offset);

    /** Reverse order of selected frames
     * @return true if all frames were successfully reversed, otherwise will return false.
     */
    bool reverseOrderOfSelection();

    void deselectAll();

    bool moveSelectedFrames(int offset);
    QList<int> getSelectedFramesByPos() const { return mSelectedFrames_byPosition; }

    /** Predetermines whether the frames can be moved to a new position depending on the offset
     *
     * @param offset Should be start press position - current position
     * @return true if selected frames can be moved otherwise false
     */
    bool canMoveSelectedFramesToOffset(int offset) const;

    Status save(const QString& sDataFolder, QStringList& attachedFiles, ProgressCallback progressStep);
    virtual Status presave(const QString& sDataFolder) { Q_UNUSED(sDataFolder); return Status::SAFE; }

    bool isPaintable() const;

    /** Returns a list of dirty frame positions */
    QList<int> dirtyFrames() const { return mDirtyFrames; }

    /** Mark the frame position as dirty.
     *  Any operation causing the frame to be modified, added, updated or removed, should call this. */
    void markFrameAsDirty(const int frameNumber) { mDirtyFrames << frameNumber; }

    /** Clear the list of dirty keyframes */
    void clearDirtyFrames() { mDirtyFrames.clear(); }

protected:
    virtual KeyFrame* createKeyFrame(int position) = 0;
    bool loadKey(KeyFrame*);

private:
    void removeFromSelectionList(int position);

    LAYER_TYPE meType = UNDEFINED;
    int        mId = 0;
    bool       mVisible = true;
    qreal      mOpacity = 1.0;
    bool       mLocked = false;
    bool       mClipMask = false;
    QString    mName;
    int        mColorIndex = -1;

    std::map<int, KeyFrame*, std::greater<int>> mKeyFrames;

    // We need to keep track of selected frames ordered by last selected
    // and by position.
    // Both should be pre-sorted on each selection for optimization purpose when moving frames.
    //
    QList<int> mSelectedFrames_byLast; // Used to handle selection range (based on last selected
    QList<int> mSelectedFrames_byPosition; // Used to handle frames movements on the timeline

    // Used for clearing cache for modified frames.
    QList<int> mDirtyFrames;
};

#endif
