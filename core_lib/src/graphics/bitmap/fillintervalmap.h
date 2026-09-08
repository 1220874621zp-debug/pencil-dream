/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

Scanline fill interval primitives, adapted from Krita's
KisFillInterval / KisFillIntervalMap (GPL-2.0-or-later,
Copyright 2014 Dmitry Kazakov <dimula73@gmail.com>).

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef FILLINTERVALMAP_H
#define FILLINTERVALMAP_H

#include <QStack>
#include <QHash>
#include <QMap>

/** A horizontal run of pixels on a single row, used by the scanline fill. */
struct FillInterval
{
    FillInterval()
        : start(0), end(-1), row(-1)
    {
    }

    FillInterval(int _start, int _end, int _row)
        : start(_start), end(_end), row(_row)
    {
    }

    inline void invalidate() { end = start - 1; }
    inline bool isValid() const { return end >= start; }
    inline int width() const { return end - start + 1; }

    int start;
    int end;
    int row;
};

/**
 * Index of "backward" intervals of the scanline fill algorithm: the
 * already-visited spans of the previous direction, used to avoid processing
 * the same pixels twice after a direction swap.
 */
class FillIntervalMap
{
public:
    FillIntervalMap();

    void insertInterval(const FillInterval& interval);

    /** Crops the interval so that it does not overlap any stored interval of the same row. */
    void cropInterval(FillInterval* interval);

    QStack<FillInterval> fetchAllIntervals(int rowCorrection) const;

    void clear();

private:
    struct Private;
    Private* m_d;
};

#endif // FILLINTERVALMAP_H
