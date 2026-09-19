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
class QPushButton;
class QRadioButton;
class QSlider;
class QSpinBox;
class QTimer;
class QVBoxLayout;
class Editor;

/** 自动上阴影参数对话框（CSP 参数模型）：左参数右预览。
    光源/置换控制场生成段；阈值/类型/色阶控制映射段（4 阶独立色+混合模式）。 */
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
                      QDoubleSpinBox*& spinOut, QSlider*& sliderOut);
    void pickLevelColor(const int levelIndex);
    void updateLevelButton(const int levelIndex);
    void grabPreviewSource();
    void schedulePreview();
    void renderPreview();

    QVBoxLayout* mParamColumn = nullptr;

    QDoubleSpinBox* mAngleSpin = nullptr;
    QDoubleSpinBox* mDistanceSpin = nullptr;
    QDoubleSpinBox* mDisplaceSpin = nullptr;
    QDoubleSpinBox* mFeatherSpin = nullptr;
    QSlider* mFeatherSlider = nullptr;
    QComboBox* mTypeCombo = nullptr;
    QCheckBox* mInvertCheck = nullptr;
    QSpinBox* mThresholdSpins[3] = { nullptr, nullptr, nullptr };
    QPushButton* mLevelButtons[4] = { nullptr, nullptr, nullptr, nullptr };
    QComboBox* mLevelCombos[4] = { nullptr, nullptr, nullptr, nullptr };
    QRadioButton* mCurrentFrameRadio = nullptr;
    QRadioButton* mAllKeyFramesRadio = nullptr;

    QLabel* mPreviewLabel = nullptr;
    QTimer* mPreviewTimer = nullptr;
    Editor* mEditor = nullptr;
    QImage mScaledSource;        // 当前帧缩放到预览框尺寸的副本（预览基准，COW 不动原图）
    double mPreviewScale = 1.0;  // 预览缩放比：像素参数按此同比后预览才与实跑一致
    bool mPreviewOriginal = false;

    AutoShadowLevel mLevels[4];  // 色阶 1..4（颜色+混合模式）
};

#endif // AUTOSHADOWDIALOG_H
