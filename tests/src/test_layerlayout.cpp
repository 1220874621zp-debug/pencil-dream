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
#include "catch.hpp"

#include <QSet>
#include <QMap>
#include <algorithm>

#include "layer.h"
#include "layerbitmap.h"
#include "keyframe.h"

namespace
{
    QList<int> positionsOf(const Layer& layer)
    {
        QList<int> positions;
        layer.foreachKeyFrame([&positions](KeyFrame* key) { positions.append(key->pos()); });
        std::sort(positions.begin(), positions.end());
        return positions;
    }

    void setExplicit(Layer* layer, int pos, int len)
    {
        KeyFrame* key = layer->getKeyFrameAt(pos);
        REQUIRE(key != nullptr);
        key->setLength(len);
        key->setLengthExplicit(true);
    }
}

TEST_CASE("Layer Layout Snapshot and Restore")
{
    LayerBitmap layer(1);
    REQUIRE(layer.addNewKeyFrameAt(1));
    REQUIRE(layer.addNewKeyFrameAt(5));
    REQUIRE(layer.addNewKeyFrameAt(9));

    const auto before = layer.captureKeyFrameLayout();
    REQUIRE(before.count() == 3);

    SECTION("wholesale rebuild restores positions")
    {
        // parking-lot style rearrangement: 1,5,9 -> 10,20,30
        KeyFrame* k1 = layer.takeKeyFrame(1);
        KeyFrame* k5 = layer.takeKeyFrame(5);
        KeyFrame* k9 = layer.takeKeyFrame(9);
        REQUIRE(k1 != nullptr);
        layer.addKeyFrame(10, k1);
        layer.addKeyFrame(20, k5);
        layer.addKeyFrame(30, k9);
        REQUIRE(positionsOf(layer) == QList<int>({ 10, 20, 30 }));

        QList<KeyFrame*> extracted;
        layer.applyKeyFrameLayout(before, extracted);
        REQUIRE(extracted.isEmpty());
        REQUIRE(positionsOf(layer) == QList<int>({ 1, 5, 9 }));
    }

    SECTION("removed frames are resurrected by pointer identity")
    {
        KeyFrame* taken = layer.takeKeyFrame(5);
        REQUIRE(taken != nullptr);
        REQUIRE(positionsOf(layer) == QList<int>({ 1, 9 }));

        // the snapshot still lists the taken pointer: the rebuild re-hosts it
        QList<KeyFrame*> extracted;
        layer.applyKeyFrameLayout(before, extracted);
        REQUIRE(extracted.isEmpty());
        REQUIRE(positionsOf(layer) == QList<int>({ 1, 5, 9 }));
        REQUIRE(layer.containsKeyFramePointer(taken)); // layer owns it again
    }

    SECTION("apply extracts keys that the layout does not list")
    {
        layer.addNewKeyFrameAt(12);
        QList<KeyFrame*> extracted;
        layer.applyKeyFrameLayout(before, extracted);
        REQUIRE(extracted.count() == 1);
        REQUIRE(extracted.first()->pos() == 12);
        delete extracted.first();
        REQUIRE(positionsOf(layer) == QList<int>({ 1, 5, 9 }));
    }
}

TEST_CASE("Layer Gap Absorption (TVP delete semantics)")
{
    SECTION("explicit predecessor absorbs the deleted span")
    {
        LayerBitmap layer(1);
        layer.addNewKeyFrameAt(1);
        layer.addNewKeyFrameAt(5);
        layer.addNewKeyFrameAt(9);
        setExplicit(&layer, 1, 2); // block [1,3)

        REQUIRE(layer.removeKeyFrame(5));
        // 1 extends to the next remaining key (9)
        KeyFrame* prev = layer.getKeyFrameAt(1);
        REQUIRE(prev->isLengthExplicit());
        REQUIRE(prev->length() == 8);
        REQUIRE(layer.getBlockEnd(prev) == 9);
        REQUIRE(positionsOf(layer) == QList<int>({ 1, 9 }));
    }

    SECTION("auto predecessor needs no absorption")
    {
        LayerBitmap layer(1);
        layer.addNewKeyFrameAt(1);
        layer.addNewKeyFrameAt(5);
        layer.addNewKeyFrameAt(9);
        REQUIRE(layer.removeKeyFrame(5));
        KeyFrame* prev = layer.getKeyFrameAt(1);
        REQUIRE_FALSE(prev->isLengthExplicit());
        REQUIRE(layer.getBlockEnd(prev) == 9);
    }

    SECTION("absorbGapsAt skips positions that still hold a key")
    {
        LayerBitmap layer(1);
        layer.addNewKeyFrameAt(2);
        layer.addNewKeyFrameAt(6);
        setExplicit(&layer, 2, 1);

        layer.absorbGapsAt({ 6, 100 });
        // 6 still holds a key: untouched; 100 has no predecessor: untouched
        REQUIRE(layer.getKeyFrameAt(2)->length() == 1);

        KeyFrame* taken = layer.takeKeyFrame(6);
        layer.absorbGapsAt({ 6 });
        REQUIRE(layer.getKeyFrameAt(2)->length() == 1); // nothing after the gap
        delete taken;
    }
}

TEST_CASE("Hold-N Collision Push Model")
{
    // mirrors TimeLine::applyHoldLength planning: selected {10,20,30}, n=2,
    // unselected 13 sits inside the re-spaced span [10,14] and must be pushed
    const QList<int> selected = { 10, 20, 30 };
    const int n = 2;
    const int start = selected.first();
    const int tailNew = start + (selected.count() - 1) * n; // 14

    QMap<int, int> targets; // old -> new
    for (int i = 0; i < selected.count(); ++i)
    {
        targets[selected[i]] = start + i * n; // first frame keeps its spot
    }

    QSet<int> selSet(selected.cbegin(), selected.cend());
    bool pushing = false;
    int cursor = tailNew;
    int prevOld = selected.last();
    for (int pos : QList<int>({ 10, 13, 20, 30 }))
    {
        if (selSet.contains(pos) || pos <= start) { continue; }
        if (!pushing && pos > tailNew) { continue; } // safely beyond the span
        pushing = true;
        int target = cursor + (pos - prevOld);
        target = qMax(target, cursor + 1); // keep the sequence monotonic
        targets[pos] = target;
        cursor = target;
        prevOld = pos;
    }

    // selected frames land equally spaced, the intruder follows after the tail
    REQUIRE(targets[10] == 10);
    REQUIRE(targets[20] == 12);
    REQUIRE(targets[30] == 14);
    REQUIRE(targets[13] == 15);
}
