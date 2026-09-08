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
#include "colorizeimage.h"

ColorizeImage::ColorizeImage(const ColorizeImage& rhs)
    : BitmapImage(rhs),
      mColoring(rhs.mColoring),
      mColoringBounds(rhs.mColoringBounds),
      mNeedsUpdate(rhs.mNeedsUpdate)
{
}

ColorizeImage* ColorizeImage::clone() const
{
    return new ColorizeImage(*this);
}

void ColorizeImage::setModified(bool b)
{
    BitmapImage::setModified(b);
    if (b)
        mNeedsUpdate = true;
}

void ColorizeImage::setColoringResult(QImage result, QRect bounds, quint32 structureGeneration)
{
    mColoring = result;
    mColoringBounds = bounds;
    mComputedStructureGeneration = structureGeneration;
    mNeedsUpdate = false;
}
