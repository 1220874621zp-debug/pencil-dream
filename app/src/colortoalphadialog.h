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

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/
#ifndef COLORTOALPHADIALOG_H
#define COLORTOALPHADIALOG_H

#include <QRgb>
#include <QDialog>

#include "colortoalpha.h"

class QDoubleSpinBox;
class QPushButton;
class QRadioButton;
class QSlider;

/** 颜色转透明度参数对话框（Krita Color to Alpha 移植） */
class ColorToAlphaDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ColorToAlphaDialog(QWidget* parent = nullptr);

    ColorToAlphaParams params() const;
    bool applyToAllKeyFrames() const;

private:
    void pickTargetColor();
    void updateColorButton();

    QPushButton* mColorButton = nullptr;
    QSlider* mThresholdSlider = nullptr;
    QDoubleSpinBox* mThresholdSpin = nullptr;
    QRadioButton* mCurrentFrameRadio = nullptr;
    QRadioButton* mAllKeyFramesRadio = nullptr;

    QRgb mTargetColor = qRgb(255, 255, 255);
};

#endif // COLORTOALPHADIALOG_H
