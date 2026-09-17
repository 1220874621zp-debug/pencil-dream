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
#ifndef LAYERSPLITDIALOG_H
#define LAYERSPLITDIALOG_H

#include <QDialog>

#include "layersplitter.h"

class QCheckBox;
class QDoubleSpinBox;
class QRadioButton;
class QSlider;

/** 拆分图层颜色参数对话框（Krita Split Layer 移植） */
class LayerSplitDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LayerSplitDialog(QWidget* parent = nullptr);

    LayerSplitParams params() const;
    bool applyToAllKeyFrames() const;

private:
    QSlider* mFuzzinessSlider = nullptr;
    QDoubleSpinBox* mFuzzinessSpin = nullptr;
    QCheckBox* mDisregardOpacityCheck = nullptr;
    QCheckBox* mSortLayersCheck = nullptr;
    QCheckBox* mHideOriginalCheck = nullptr;
    QRadioButton* mCurrentFrameRadio = nullptr;
    QRadioButton* mAllKeyFramesRadio = nullptr;
};

#endif // LAYERSPLITDIALOG_H
