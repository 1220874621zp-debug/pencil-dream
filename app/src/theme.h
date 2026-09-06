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

#ifndef THEME_H
#define THEME_H

#include <QColor>

class QApplication;

/**
 * Modern dark theme (Procreate-Dreams-inspired).
 * Single source of truth for colors used by both the global
 * stylesheet and the custom-painted widgets (timeline etc.).
 */
namespace Theme
{
    // Base palette
    extern const QColor Window;          // app background
    extern const QColor Panel;           // dock / panel background
    extern const QColor PanelRaised;     // raised buttons, inputs
    extern const QColor PanelHover;      // hover state
    extern const QColor Border;          // hairline borders
    extern const QColor TextPrimary;     // main text
    extern const QColor TextSecondary;   // dimmed text
    extern const QColor Accent;          // crimson accent (selection, playhead)
    extern const QColor AccentHover;     // accent hover state

    // Layer type colors (timeline tracks, layer rows)
    extern const QColor LayerBitmap;
    extern const QColor LayerSound;
    extern const QColor LayerCamera;

    // Timeline specifics (custom-painted, does not follow QSS)
    extern const QColor TimelineBackground;
    extern const QColor TimelineRowAlternate;
    extern const QColor TimelineFrameBorder;
    extern const QColor TimelineFrameFill;
    extern const QColor TimelineSelectedFrameFill;
    extern const QColor TimelineCurrentFrameBorder;
    extern const QColor TimelinePlayhead;

    /// Applies Fusion style + dark palette + global stylesheet to the app.
    void applyDarkTheme(QApplication& app);
}

#endif
