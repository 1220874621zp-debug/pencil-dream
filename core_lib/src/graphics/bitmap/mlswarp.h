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

#ifndef MLSWARP_H
#define MLSWARP_H

#include <QImage>
#include <QPointF>
#include <QVector>
#include <QRectF>

/**
 * Moving-Least-Squares image deformation, ported from Krita's
 * KisWarpTransformWorker (Schaefer et al., "Image Deformation Using
 * Moving Least Squares"). Rigid mode keeps local scale intact and is
 * the default in Krita's warp transform.
 */
namespace MlsWarp
{
    /** Maps a single point through the MLS deformation defined by control
     *  point pairs (orig -> moved). */
    QPointF rigidTransformMath(const QPointF& v,
                               const QVector<QPointF>& origPoints,
                               const QVector<QPointF>& transfPoints,
                               qreal alpha);

    /** Warps srcImage according to the control point pairs. Control point
     *  coordinates are local to the source image (0,0 = top-left pixel).
     *  Returns the warped image; newOffset (if set) receives the top-left
     *  position of the result relative to the source image origin. */
    QImage warpImage(const QImage& srcImage,
                     const QVector<QPointF>& origPoints,
                     const QVector<QPointF>& transfPoints,
                     qreal alpha,
                     bool smoothSampling,
                     QPointF* newOffset = nullptr);
}

#endif // MLSWARP_H
