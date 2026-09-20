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
#include <QImage>
#include <QRgb>

#include "autoshadow.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLayout;
class QPushButton;
class QRadioButton;
class QSlider;
class QTimer;
class QVBoxLayout;
class Editor;
class LevelsBar;

/** 自动上阴影参数对话框（CSP 参数模型 + PS 交互）：左参数右预览。
    预设一键整套光源+色带（CSP 预设语义）；光源列表可增删（CSP 添加光源同款），
    滑杆编辑选中光源；预览框点击/拖拽定位选中光源（点其他标记切换选中）；
    色阶阈值=渐变条拖块（点色段改该阶颜色）。 */
class AutoShadowDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AutoShadowDialog(Editor* editor, QWidget* parent = nullptr);

    bool eventFilter(QObject* watched, QEvent* event) override;

    AutoShadowParams params() const;
    bool applyToAllKeyFrames() const;

private:
    void addSliderRow(const QString& labelText, int minV, int maxV, int defV, const QString& tip,
                      const QString& suffix, QDoubleSpinBox*& spinOut, QSlider*& sliderOut);
    void addSliderRowTo(QLayout* layout, const QString& labelText, int minV, int maxV, int defV,
                        const QString& tip, const QString& suffix, QDoubleSpinBox*& spinOut, QSlider*& sliderOut);
    void pickLevelColor(const int levelIndex);
    void updateLevelButton(const int levelIndex);
    void grabPreviewSource();
    void schedulePreview();
    void renderPreview();
    void setLightFromPreview(const QPoint& pos);
    void syncLevelsBar();
    void syncHatchEnabled();
    void refreshLightCombo();
    void syncLightControls();
    void syncLevelRow(const int levelIndex);
    void applyPreset(const int presetIndex);

    QVBoxLayout* mParamColumn = nullptr;

    QComboBox* mPresetCombo = nullptr;
    QComboBox* mLightCombo = nullptr;
    QPushButton* mAddLightButton = nullptr;
    QPushButton* mRemoveLightButton = nullptr;
    QVector<AutoShadowLight> mLights;
    int mCurrentLight = 0;
    bool mApplyingPreset = false;

    QDoubleSpinBox* mLightXSpin = nullptr;
    QDoubleSpinBox* mLightYSpin = nullptr;
    QDoubleSpinBox* mLightHeightSpin = nullptr;
    QDoubleSpinBox* mLightIntensitySpin = nullptr;
    QSlider* mLightXSlider = nullptr;
    QSlider* mLightYSlider = nullptr;
    QSlider* mLightHeightSlider = nullptr;
    QSlider* mLightIntensitySlider = nullptr;
    QDoubleSpinBox* mThresholdSpin = nullptr;
    QDoubleSpinBox* mChokeSpin = nullptr;
    QDoubleSpinBox* mGradientSpin = nullptr;
    QDoubleSpinBox* mNormalSpin = nullptr;
    QDoubleSpinBox* mFormHeightSpin = nullptr;
    QDoubleSpinBox* mFormRadiusSpin = nullptr;
    QDoubleSpinBox* mFormSmoothSpin = nullptr;
    QDoubleSpinBox* mOcclusionSpin = nullptr;
    QDoubleSpinBox* mFeatherSpin = nullptr;
    QSlider* mFeatherSlider = nullptr;
    QComboBox* mTypeCombo = nullptr;
    QCheckBox* mInvertCheck = nullptr;
    QCheckBox* mHatchCheck = nullptr;
    QDoubleSpinBox* mHatchAngleSpin = nullptr;
    QDoubleSpinBox* mHatchSpacingSpin = nullptr;
    QSlider* mHatchAngleSlider = nullptr;
    QSlider* mHatchSpacingSlider = nullptr;
    LevelsBar* mLevelsBar = nullptr;
    QPushButton* mLevelButtons[4] = { nullptr, nullptr, nullptr, nullptr };
    QComboBox* mLevelCombos[4] = { nullptr, nullptr, nullptr, nullptr };
    QDoubleSpinBox* mLevelOpacitySpins[4] = { nullptr, nullptr, nullptr, nullptr };
    QRadioButton* mCurrentFrameRadio = nullptr;
    QRadioButton* mAllKeyFramesRadio = nullptr;

    QLabel* mPreviewLabel = nullptr;
    QComboBox* mViewCombo = nullptr;
    QPushButton* mCompareButton = nullptr;
    QTimer* mPreviewTimer = nullptr;
    Editor* mEditor = nullptr;
    QImage mScaledSource;        // 当前帧缩放到预览框尺寸的副本（预览基准，COW 不动原图）
    double mPreviewScale = 1.0;  // 预览缩放比：像素参数按此同比后预览才与实跑一致
    bool mDraggingLight = false;

    AutoShadowLevel mLevels[4];  // 色阶 1..4（颜色+混合模式）
};

#endif // AUTOSHADOWDIALOG_H
