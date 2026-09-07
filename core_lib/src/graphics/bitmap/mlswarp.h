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
#include <functional>

/**
 * Deformation math ported from Krita:
 *  - Moving-Least-Squares warp (KisWarpTransformWorker: rigid / similitude / affine)
 *  - Green Coordinates cage transform (KisGreenCoordinatesMath + KisCageTransformWorker)
 *  - Liquify brush dabs (KisLiquifyTransformWorker + KisLiquifyPaintop)
 *  - Perspective quad mapping (QTransform::quadToQuad)
 */
namespace MlsWarp
{
    enum class WarpType { Affine, Similitude, Rigid };

    enum class LiquifyOp { Move, Scale, Rotate, Offset, Undo };

    /** MLS control-point deformation. Control point coordinates are local
     *  to the source image (0,0 = top-left pixel). */
    QPointF warpTransformMath(WarpType type,
                              const QPointF& v,
                              const QVector<QPointF>& origPoints,
                              const QVector<QPointF>& transfPoints,
                              qreal alpha);

    /** Warps srcImage with the MLS deformation. newOffset (if set) receives
     *  the top-left of the result relative to the source image origin. */
    QImage warpImage(const QImage& srcImage,
                     const QVector<QPointF>& origPoints,
                     const QVector<QPointF>& transfPoints,
                     qreal alpha,
                     bool smoothSampling,
                     QPointF* newOffset = nullptr,
                     WarpType type = WarpType::Rigid);

    /** Green-coordinates cage transform: origCage/transfCage vertices are in
     *  source-image local coordinates. Faithful port of Krita's cage worker
     *  (only grid points inside the original cage are moved). */
    QImage cageWarpImage(const QImage& srcImage,
                         const QVector<QPointF>& origCage,
                         const QVector<QPointF>& transfCage,
                         bool smoothSampling,
                         QPointF* newOffset = nullptr);

    /** One liquify brush dab, applied in place to workImage (local coords).
     *  base = brush center; sigma = brush size (Krita semantics);
     *  direction = unit drawing direction (for Move/Offset it is pre-multiplied
     *  with size*amount by the caller in Krita; here pass the raw dab vector
     *  for Move/Offset and the scalar amount for Scale/Rotate/Undo);
     *  original = pre-stroke image for the Undo op (may be null otherwise).
     *  Gaussian falloff lambda = exp(-0.5*(d/sigma)^2), maxDist = 3*sigma
     *  exactly like KisLiquifyTransformWorker. */
    void liquifyDab(QImage& workImage,
                    const QImage& original,
                    const QPointF& base,
                    qreal sigma,
                    LiquifyOp op,
                    const QPointF& dabVector,
                    qreal amount);

    /** Renders a deformation defined by an explicitly moved lattice
     *  (row-major, (cols+1)*(rows+1) points, cols = ceil(width/cellSize)).
     *  This is the liquify data path: dabs move lattice points, rendering
     *  resamples the image once per frame like Krita's grid strategy. */
    QImage gridWarpImage(const QImage& srcImage,
                         const QVector<QPointF>& movedLattice,
                         int cellSize,
                         bool smoothSampling,
                         QPointF* newOffset = nullptr);

    /** Perspective (4-point) mapping: maps the source rectangle quad to the
     *  given destination quad. Both in source-image local coordinates. */
    QImage perspectiveWarpImage(const QImage& srcImage,
                                const QPolygonF& dstQuad,
                                bool smoothSampling,
                                QPointF* newOffset = nullptr);
}

#endif // MLSWARP_H
