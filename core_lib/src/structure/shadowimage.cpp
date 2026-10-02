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
#include "shadowimage.h"

ShadowImage::ShadowImage(const ShadowImage& rhs)
    : BitmapImage(rhs),
      mShadow(rhs.mShadow),
      mShadowBounds(rhs.mShadowBounds),
      mNeedsUpdate(rhs.mNeedsUpdate),
      mComputedStructureGeneration(rhs.mComputedStructureGeneration),
      mWarnings(rhs.mWarnings)
{
}

ShadowImage* ShadowImage::clone() const
{
    return new ShadowImage(*this);
}

void ShadowImage::setModified(bool b)
{
    BitmapImage::setModified(b);
    if (b)
        mNeedsUpdate = true;
}

void ShadowImage::setShadowResult(QImage result, QRect bounds, quint32 structureGeneration)
{
    mShadow = result;
    mShadowBounds = bounds;
    mNeedsUpdate = false;
    mComputedStructureGeneration = structureGeneration;
}
