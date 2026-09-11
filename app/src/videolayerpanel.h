#ifndef VIDEOLAYERPANEL_H
#define VIDEOLAYERPANEL_H

#include "basedockwidget.h"

class QDoubleSpinBox;
class QLabel;
class LayerVideo;

// 参考视频层的 AE 式属性栏:位移 X/Y + 缩放%。
// 选中参考视频层时可编辑,其他层时控件灰显常驻(防排版跳动)。
class VideoLayerPanel : public BaseDockWidget
{
    Q_OBJECT

public:
    explicit VideoLayerPanel(QWidget* parent);
    ~VideoLayerPanel() override;

    void initUI() override;
    void updateUI() override;

private slots:
    void offsetXChanged(double);
    void offsetYChanged(double);
    void scaleChanged(double);

private:
    void makeConnections();
    LayerVideo* currentVideoLayer() const;

    QDoubleSpinBox* mOffsetXSpin = nullptr;
    QDoubleSpinBox* mOffsetYSpin = nullptr;
    QDoubleSpinBox* mScaleSpin = nullptr;   // 百分比显示,层内存系数
    QLabel* mHintLabel = nullptr;
    bool mUpdatingUI = false;               // 回填时挡住 valueChanged 回环
};

#endif // VIDEOLAYERPANEL_H
