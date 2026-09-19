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
#ifndef AUTOSHADOWDIALOG_H
#define AUTOSHADOWDIALOG_H

#include <QDialog>
#include <QRgb>

#include "autoshadow.h"

class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSlider;

/** 自动上阴影参数对话框（光源角度/距离、阴影范围、单双层、阴影色、浓度、阻塞、作用范围） */
class AutoShadowDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AutoShadowDialog(QWidget* parent = nullptr);

    AutoShadowParams params() const;
    bool applyToAllKeyFrames() const;

private:
    void pickShadowColor();
    void updateColorButton();
    QDoubleSpinBox* addSliderRow(const QString& labelText, int minV, int maxV, int defV, const QString& tip);

    QPushButton* mColorButton = nullptr;
    QDoubleSpinBox* mAngleSpin = nullptr;
    QDoubleSpinBox* mDistanceSpin = nullptr;
    QDoubleSpinBox* mRangeSpin = nullptr;
    QRadioButton* mSingleLevelRadio = nullptr;
    QRadioButton* mTwoLevelRadio = nullptr;
    QLabel* mSecondLabel = nullptr;
    QSlider* mSecondSlider = nullptr;
    QDoubleSpinBox* mSecondSpin = nullptr;
    QDoubleSpinBox* mOpacitySpin = nullptr;
    QDoubleSpinBox* mChokeSpin = nullptr;
    QRadioButton* mCurrentFrameRadio = nullptr;
    QRadioButton* mAllKeyFramesRadio = nullptr;

    QRgb mShadowColor = qRgb(150, 130, 200);
};

#endif // AUTOSHADOWDIALOG_H
