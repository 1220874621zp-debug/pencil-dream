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

#include "backgroundwidget.h"

#include <QStyleOption>
#include <QPainter>
#include <QPaintEvent>

#include "editor.h"
#include "layermanager.h"
#include "viewmanager.h"
#include "layercamera.h"


BackgroundWidget::BackgroundWidget(QWidget* parent) : QWidget(parent)
{
    setObjectName("BackgroundWidget");

    // Qt::WA_StaticContents ensure that the widget contents are rooted to the top-left corner
    // and don't change when the widget is resized.
    setAttribute( Qt::WA_StaticContents );
}

BackgroundWidget::~BackgroundWidget()
{
}

void BackgroundWidget::init(PreferenceManager *prefs)
{
    mPrefs = prefs;
    connect(mPrefs, &PreferenceManager::optionChanged, this, &BackgroundWidget::settingUpdated);

    loadBackgroundStyle();
    mHasShadow = mPrefs->isOn(SETTING::SHADOW);

    update();
}

void BackgroundWidget::settingUpdated(SETTING setting)
{
    switch ( setting )
    {
    case SETTING::BACKGROUND_STYLE:

        loadBackgroundStyle();
        update();
        break;

    case SETTING::SHADOW:

        mHasShadow = mPrefs->isOn(SETTING::SHADOW);
        update();
        break;

    default:
        break;
    }
}

void BackgroundWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setClipRect(event->rect());

    // Friction-style workspace backdrop: vertical gradient easing into
    // grey, with a faint fine grid — visible around the paper only
    const QRect r = rect();
    QLinearGradient grad(QPointF(0, 0), QPointF(0, r.height()));
    grad.setColorAt(0.0,  QColor(0x0E, 0x0F, 0x11));
    grad.setColorAt(0.68, QColor(0x15, 0x16, 0x17));
    grad.setColorAt(1.0,  QColor(0x26, 0x28, 0x2C));
    painter.fillRect(r, grad);

    // Paper follows the camera frame: the camera rect in document space,
    // mapped through the camera transform and the view transform, so it
    // scales/pans together with the camera border instead of staying fixed.
    QPolygonF paperPoly;
    bool havePaper = false;
    if (mEditor != nullptr && mEditor->layers() != nullptr)
    {
        LayerCamera* cam = mEditor->layers()->getCameraLayerBelow(mEditor->currentLayerIndex());
        if (cam != nullptr)
        {
            const QTransform camT = cam->getViewAtFrame(mEditor->currentFrame()).inverted();
            const QPolygonF camPoly = camT.map(QPolygonF(QRectF(cam->getViewRect())));
            for (const QPointF& pnt : camPoly)
                paperPoly << mEditor->view()->mapCanvasToScreen(pnt);
            havePaper = paperPoly.size() == 4;
        }
    }
    const QRect paper = havePaper ? paperPoly.boundingRect().toAlignedRect()
                                  : r.adjusted(24, 24, -24, -24);
    const QRect workspace = r;

    painter.setClipRegion(QRegion(workspace).subtracted(QRegion(paper)));
    painter.setPen(QPen(QColor(0x26, 0x26, 0x26), 1.0));
    const int spacing = 16;
    for (int x = 0; x <= r.width(); x += spacing)
        painter.drawLine(x, 0, x, r.height());
    for (int y = 0; y <= r.height(); y += spacing)
        painter.drawLine(0, y, r.width(), y);

    // clean light paper for drawing (strokes stay readable on it);
    // sharp corners — plain paper, no scene-card styling
    painter.setClipRect(event->rect());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0xFF, 0xFF, 0xFF));
    if (havePaper)
    {
        const QRectF b = paperPoly.boundingRect();
        const bool axisAligned = paperPoly.size() == 4
            && qFuzzyCompare(paperPoly.at(0).x(), b.left()) && qFuzzyCompare(paperPoly.at(0).y(), b.top())
            && qFuzzyCompare(paperPoly.at(1).x(), b.right()) && qFuzzyCompare(paperPoly.at(1).y(), b.top())
            && qFuzzyCompare(paperPoly.at(2).x(), b.right()) && qFuzzyCompare(paperPoly.at(2).y(), b.bottom())
            && qFuzzyCompare(paperPoly.at(3).x(), b.left()) && qFuzzyCompare(paperPoly.at(3).y(), b.bottom());
        if (axisAligned)
            painter.drawRect(b);
        else
            painter.drawPolygon(paperPoly);
    }
    else
    {
        painter.drawRect(paper);
    }
    painter.setRenderHint(QPainter::Antialiasing, false);

    if (mHasShadow)
        drawShadow(painter);
}

void BackgroundWidget::loadBackgroundStyle()
{
    // background is now fully custom-painted (gradient + grid);
    // no stylesheet background anymore, keep the entry point for the
    // preference-change notification chain
    setStyleSheet("");
}

void BackgroundWidget::drawShadow(QPainter& painter)
{
    int radius1 = 12;
    int radius2 = 8;

    QColor color = Qt::black;
    qreal opacity = 0.15;

    QLinearGradient shadow = QLinearGradient( 0, 0, 0, radius1 );

    int r = color.red();
    int g = color.green();
    int b = color.blue();
    qreal a = color.alphaF();
    shadow.setColorAt( 0.0, QColor( r, g, b, qRound( a * 255 * opacity ) ) );
    shadow.setColorAt( 1.0, QColor( r, g, b, 0 ) );

    painter.setPen( Qt::NoPen );
    painter.setBrush( shadow );
    painter.drawRect( QRect( 0, 0, width(), radius1 ) );

    shadow.setFinalStop( radius1, 0 );
    painter.setBrush( shadow );
    painter.drawRect( QRect( 0, 0, radius1, height() ) );

    shadow.setStart( 0, height() );
    shadow.setFinalStop( 0, height() - radius2 );
    painter.setBrush( shadow );
    painter.drawRect( QRect( 0, height() - radius2, width(), height() ) );

    shadow.setStart( width(), 0 );
    shadow.setFinalStop( width() - radius2, 0 );
    painter.setBrush( shadow );
    painter.drawRect( QRect( width() - radius2, 0, width(), height() ) );
}
