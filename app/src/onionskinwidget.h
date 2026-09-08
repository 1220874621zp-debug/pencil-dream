/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef ONIONSKINWIDGET_H
#define ONIONSKINWIDGET_H

#include "basedockwidget.h"

namespace Ui
{
    class OnionSkin;
}

class Editor;
class QToolButton;
class QSlider;
class QDoubleSpinBox;
class ViewManager;

class OnionSkinWidget : public BaseDockWidget
{
    Q_OBJECT

public:
    explicit OnionSkinWidget(QWidget* parent);
    virtual ~OnionSkinWidget() override;

    void initUI() override;
    void updateUI() override;

private slots:
    void playbackStateChanged(int);
    void onionBlueButtonClicked(bool);
    void onionRedButtonClicked(bool);
    void onionMaxOpacityChange(int);
    void onionMinOpacityChange(int);
    void onionPrevFramesNumChange(int);
    void onionNextFramesNumChange(int);
    void onionSkinModeChange(int);
    void onionSkinMultipleLayersEnabled(bool value);
    void onionToggleClicked(bool);

private:
    void makeConnections();
    void buildParamRows();

    Ui::OnionSkin* ui = nullptr;

    // 灯泡开关：同时开启/关闭前后帧洋葱皮
    QToolButton* mOnionToggleButton = nullptr;

    // 滑杆+输入框参数行（输入框为数据源，滑杆双向同步）
    QSlider* mPrevFramesSlider = nullptr;
    QDoubleSpinBox* mPrevFramesSpin = nullptr;
    QSlider* mNextFramesSlider = nullptr;
    QDoubleSpinBox* mNextFramesSpin = nullptr;
    QSlider* mMaxOpacitySlider = nullptr;
    QDoubleSpinBox* mMaxOpacitySpin = nullptr;
    QSlider* mMinOpacitySlider = nullptr;
    QDoubleSpinBox* mMinOpacitySpin = nullptr;

    QToolButton* mOnionRedButton = nullptr;
    QToolButton* mOnionBlueButton = nullptr;
};

#endif // ONIONSKINWIDGET_H
