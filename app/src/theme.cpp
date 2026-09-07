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

#include "theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

namespace Theme
{
    const QColor Window         = QColor(0x0D, 0x0D, 0x0F);
    const QColor Panel          = QColor(0x16, 0x16, 0x18);
    const QColor PanelRaised    = QColor(0x1E, 0x1E, 0x22);
    const QColor PanelHover     = QColor(0x28, 0x28, 0x2E);
    const QColor Border         = QColor(0x2A, 0x2A, 0x2E);
    const QColor TextPrimary    = QColor(0xE8, 0xE8, 0xEA);
    const QColor TextSecondary  = QColor(0x8A, 0x8A, 0x90);
    const QColor Accent         = QColor(0xE8, 0x38, 0x5A);
    const QColor AccentHover    = QColor(0xFF, 0x4D, 0x6D);

    const QColor LayerBitmap    = QColor(0x4F, 0x8C, 0xFF);
    const QColor LayerSound     = QColor(0xFF, 0x8D, 0x70);
    const QColor LayerCamera    = QColor(0xFD, 0xCA, 0x5C);

    const QColor LayerLabelColors[8] =
    {
        QColor(0x22, 0xC5, 0x5E), // 0 green
        QColor(0x06, 0xB6, 0xD4), // 1 cyan
        QColor(0xA8, 0x55, 0xF7), // 2 purple
        QColor(0xF9, 0x73, 0x16), // 3 orange
        QColor(0xF4, 0x3F, 0x5E), // 4 rose
        QColor(0x3B, 0x82, 0xF6), // 5 blue
        QColor(0xEA, 0xB3, 0x08), // 6 yellow
        QColor(0xEC, 0x48, 0x99), // 7 pink
    };

    const QColor TimelineBackground         = QColor(0x16, 0x16, 0x18);
    const QColor TimelineRowAlternate       = QColor(0x1A, 0x1A, 0x1E);
    const QColor TimelineFrameBorder        = QColor(0x2A, 0x2A, 0x2E);
    const QColor TimelineFrameFill          = QColor(0x12, 0x12, 0x16);
    const QColor TimelineSelectedFrameFill  = QColor(0x3A, 0x24, 0x2C);
    const QColor TimelineCurrentFrameBorder = QColor(0xE8, 0xE8, 0xEA);
    const QColor TimelinePlayhead           = QColor(0xE8, 0x38, 0x5A);

    static const char* kStyleSheet = R"STYLE(
/* ===== Global =====
   NOTE: no bare "QWidget { background ... }" rule here — it would force
   styled backgrounds onto custom-painted widgets (ScribbleArea, TimeLineCells).
   The base surface colors come from the QPalette set in applyDarkTheme(). */

QMainWindow::separator {
    background-color: #0D0D0F;
    width: 3px;
    height: 3px;
}

/* ===== Menus ===== */
QMenuBar {
    background-color: #0D0D0F;
    color: #B8B8BC;
    padding: 2px 4px;
}
QMenuBar::item {
    padding: 4px 10px;
    border-radius: 6px;
    background: transparent;
}
QMenuBar::item:selected { background-color: #28282E; color: #E8E8EA; }
QMenu {
    background-color: #1E1E22;
    border: 1px solid #2A2A2E;
    border-radius: 10px;
    padding: 6px 4px;
}
QMenu::item {
    padding: 6px 26px 6px 16px;
    border-radius: 6px;
    color: #E8E8EA;
}
QMenu::item:selected { background-color: #E8385A; color: #FFFFFF; }
QMenu::item:disabled { color: #5A5A60; }
QMenu::separator { height: 1px; background: #2A2A2E; margin: 5px 8px; }

/* ===== Toolbars ===== */
QToolBar {
    background-color: #161618;
    border: none;
    border-bottom: 1px solid #26262A;
    padding: 3px 6px;
    spacing: 3px;
}
QToolBar::separator { width: 1px; background: #2A2A2E; margin: 4px 6px; }
QToolButton {
    background-color: transparent;
    color: #B8B8BC;
    border: none;
    border-radius: 6px;
    padding: 4px;
}
QToolButton:hover { background-color: #28282E; color: #E8E8EA; }
QToolButton:pressed { background-color: #222226; }
QToolButton:checked { background-color: #E8385A; color: #FFFFFF; }

/* ===== Buttons ===== */
QPushButton {
    background-color: #1E1E22;
    color: #E8E8EA;
    border: 1px solid #2A2A2E;
    border-radius: 6px;
    padding: 5px 14px;
}
QPushButton:hover { background-color: #28282E; border-color: #3A3A40; }
QPushButton:pressed { background-color: #16161A; }
QPushButton:disabled { color: #5A5A60; background-color: #16161A; }

/* ===== Inputs ===== */
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
    background-color: #1E1E22;
    color: #E8E8EA;
    border: 1px solid #2A2A2E;
    border-radius: 6px;
    padding: 3px 8px;
    selection-background-color: #E8385A;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
    border-color: #E8385A;
}
QSpinBox::up-button, QDoubleSpinBox::up-button,
QSpinBox::down-button, QDoubleSpinBox::down-button {
    background-color: transparent;
    border: none;
    width: 16px;
}
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
    background-color: #1E1E22;
    border: 1px solid #2A2A2E;
    border-radius: 6px;
    selection-background-color: #E8385A;
    selection-color: #FFFFFF;
    outline: none;
    padding: 2px;
}

/* ===== Sliders ===== */
QSlider::groove:horizontal {
    height: 4px;
    background: #2A2A2E;
    border-radius: 2px;
}
QSlider::sub-page:horizontal {
    background: #E8385A;
    border-radius: 2px;
}
QSlider::handle:horizontal {
    background: #E8E8EA;
    width: 12px;
    height: 12px;
    margin: -5px 0;
    border-radius: 6px;
}
QSlider::handle:horizontal:hover { background: #FFFFFF; }

/* ===== Scrollbars ===== */
QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: #2E2E34;
    border-radius: 5px;
    min-height: 30px;
    margin: 2px;
}
QScrollBar::handle:vertical:hover { background: #3A3A42; }
QScrollBar:horizontal {
    background: transparent;
    height: 10px;
    margin: 0;
}
QScrollBar::handle:horizontal {
    background: #2E2E34;
    border-radius: 5px;
    min-width: 30px;
    margin: 2px;
}
QScrollBar::handle:horizontal:hover { background: #3A3A42; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

/* ===== Tabs ===== */
QTabWidget::pane {
    border: 1px solid #26262A;
    border-radius: 8px;
    top: -1px;
}
QTabBar::tab {
    background-color: transparent;
    color: #8A8A90;
    padding: 6px 14px;
    border-top-left-radius: 8px;
    border-top-right-radius: 8px;
    margin-right: 2px;
}
QTabBar::tab:hover { color: #E8E8EA; }
QTabBar::tab:selected {
    color: #E8E8EA;
    border-bottom: 2px solid #E8385A;
}

/* ===== Group boxes / checkboxes ===== */
QGroupBox {
    border: 1px solid #26262A;
    border-radius: 8px;
    margin-top: 10px;
    padding-top: 6px;
    color: #B8B8BC;
    font-weight: 600;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 10px;
    padding: 0 4px;
}
QCheckBox { color: #E8E8EA; spacing: 7px; background: transparent; }
QCheckBox::indicator {
    width: 15px;
    height: 15px;
    border: 1px solid #3A3A40;
    border-radius: 4px;
    background-color: #1E1E22;
}
QCheckBox::indicator:hover { border-color: #8A8A90; }
QCheckBox::indicator:checked { background-color: #E8385A; border-color: #E8385A; }
QCheckBox:disabled { color: #5A5A60; }

/* ===== Lists ===== */
QListWidget {
    background-color: #161618;
    border: none;
    border-radius: 6px;
    outline: none;
}
QListWidget::item { border-radius: 4px; padding: 2px; }
QListWidget::item:selected { background-color: #E8385A; color: #FFFFFF; }

/* ===== Docks & status bar ===== */
QDockWidget {
    color: #B8B8BC;
    titlebar-close-icon: none;
    titlebar-normal-icon: none;
}
QStatusBar {
    background-color: #0D0D0F;
    color: #8A8A90;
    border-top: 1px solid #26262A;
}
QStatusBar::item { border: none; }

/* ===== Splitter & tooltip & misc ===== */
QSplitter::handle { background-color: #0D0D0F; }
QSplitter::handle:hover { background-color: #E8385A; }
QToolTip {
    background-color: #1E1E22;
    color: #E8E8EA;
    border: 1px solid #3A3A40;
    border-radius: 6px;
    padding: 4px 8px;
}
QFrame[frameShape="4"], QFrame[frameShape="5"] { color: #2A2A2E; } /* HLine/VLine */
QAbstractScrollArea { border: none; }
)STYLE";

    void applyDarkTheme(QApplication& app)
    {
        app.setStyle(QStyleFactory::create("Fusion"));

        QPalette p;
        p.setColor(QPalette::Window,          Window);
        p.setColor(QPalette::WindowText,      TextPrimary);
        p.setColor(QPalette::Base,            Panel);
        p.setColor(QPalette::AlternateBase,   TimelineRowAlternate);
        p.setColor(QPalette::ToolTipBase,     PanelRaised);
        p.setColor(QPalette::ToolTipText,     TextPrimary);
        p.setColor(QPalette::Text,            TextPrimary);
        p.setColor(QPalette::Button,          PanelRaised);
        p.setColor(QPalette::ButtonText,      TextPrimary);
        p.setColor(QPalette::BrightText,      Qt::white);
        p.setColor(QPalette::Highlight,       Accent);
        p.setColor(QPalette::HighlightedText, Qt::white);
        p.setColor(QPalette::Disabled, QPalette::Text,       QColor(0x5A, 0x5A, 0x60));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x5A, 0x5A, 0x60));
        p.setColor(QPalette::Link,            AccentHover);
        p.setColor(QPalette::Mid,             Border);
        p.setColor(QPalette::Midlight,        PanelHover);
        p.setColor(QPalette::Dark,            QColor(0x0A, 0x0A, 0x0C));
        p.setColor(QPalette::Light,           PanelHover);
        p.setColor(QPalette::Shadow,          QColor(0x06, 0x06, 0x08));
        app.setPalette(p);

        app.setStyleSheet(kStyleSheet);
    }
}
