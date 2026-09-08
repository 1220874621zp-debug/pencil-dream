/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

Post-processing filters for the bucket fill's grayscale selection mask,
adapted from Krita's kis_selection_filters.cpp (GPL-2.0-or-later;
grow/shrink/border Copyright 2004-2010 Adrian Page, Bart Coppens,
Lukáš Tvrdý; antialias filter and grow-until-darkest filter by
Dmitry Kazakov and others).

All filters operate in place on a mask of `width` x `height` bytes, where
255 means fully selected and 0 unselected.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef FILLFILTERS_H
#define FILLFILTERS_H

#include <QImage>
#include <QVector>

namespace FillFilters
{

/** Grow (dilate) the selection by a disc of the given radius. */
void growSelection(QVector<quint8>& mask, int width, int height, int radius);

/** Shrink (erode) the selection by a disc of the given radius. */
void shrinkSelection(QVector<quint8>& mask, int width, int height, int radius);

/** Blur the selection with a gaussian kernel of the given radius. */
void featherSelection(QVector<quint8>& mask, int width, int height, int radius);

/** Smooth out the jagged staircase edges of the selection. */
void antialiasSelection(QVector<quint8>& mask, int width, int height);

/**
 * Grow the selection by up to `radius` pixels, but stop the growth at the
 * darkest and/or most opaque pixels of the reference image (the lineart):
 * the fill eats the anti-aliased halo under the lines without leaking
 * past dark strokes.
 *
 * @param reference the fill region of the reference image, same size as the
 *                  mask (ARGB32_Premultiplied)
 */
void growUntilDarkestPixel(QVector<quint8>& mask, const QImage& reference,
                           int width, int height, int radius);

} // namespace FillFilters

#endif // FILLFILTERS_H
