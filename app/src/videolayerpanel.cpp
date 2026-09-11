#include "videolayerpanel.h"

#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QSignalBlocker>

#include "editor.h"
#include "layermanager.h"
#include "scribblearea.h"
#include "layervideo.h"

VideoLayerPanel::VideoLayerPanel(QWidget* parent)
    : BaseDockWidget(parent)
{
    setTitle(tr("参考视频属性"));
}

VideoLayerPanel::~VideoLayerPanel() = default;

void VideoLayerPanel::initUI()
{
    auto makeSpin = [this](double min, double max, double step) {
        auto* spin = new QDoubleSpinBox(this);
        spin->setRange(min, max);
        spin->setDecimals(1);
        spin->setSingleStep(step);
        spin->setFixedWidth(96);
        spin->setAlignment(Qt::AlignRight);
        return spin;
    };

    mOffsetXSpin = makeSpin(-99999.0, 99999.0, 10.0);
    mOffsetYSpin = makeSpin(-99999.0, 99999.0, 10.0);
    mScaleSpin = makeSpin(5.0, 800.0, 5.0);
    mScaleSpin->setSuffix(" %");

    auto* form = new QFormLayout(this);
    form->addRow(tr("位移 X"), mOffsetXSpin);
    form->addRow(tr("位移 Y"), mOffsetYSpin);
    form->addRow(tr("缩放"), mScaleSpin);

    mHintLabel = new QLabel(tr("选中参考视频层后可编辑"), this);
    mHintLabel->setStyleSheet("color:#8A8A90;");
    form->addRow(QString(), mHintLabel);

    updateUI();
    makeConnections();
}

void VideoLayerPanel::makeConnections()
{
    connect(editor()->layers(), &LayerManager::currentLayerChanged,
            this, qOverload<>(&VideoLayerPanel::updateUI));
    connect(mOffsetXSpin, &QDoubleSpinBox::valueChanged, this, &VideoLayerPanel::offsetXChanged);
    connect(mOffsetYSpin, &QDoubleSpinBox::valueChanged, this, &VideoLayerPanel::offsetYChanged);
    connect(mScaleSpin, &QDoubleSpinBox::valueChanged, this, &VideoLayerPanel::scaleChanged);
}

LayerVideo* VideoLayerPanel::currentVideoLayer() const
{
    Layer* layer = editor()->layers()->currentLayer();
    if (layer == nullptr || layer->type() != Layer::MOVIE) { return nullptr; }
    return static_cast<LayerVideo*>(layer);
}

void VideoLayerPanel::updateUI()
{
    mUpdatingUI = true;
    LayerVideo* video = currentVideoLayer();
    const bool editable = (video != nullptr);
    mOffsetXSpin->setEnabled(editable);
    mOffsetYSpin->setEnabled(editable);
    mScaleSpin->setEnabled(editable);
    mHintLabel->setVisible(!editable);
    if (video)
    {
        mOffsetXSpin->setValue(video->videoOffset().x());
        mOffsetYSpin->setValue(video->videoOffset().y());
        mScaleSpin->setValue(video->videoScale() * 100.0);
    }
    mUpdatingUI = false;
}

void VideoLayerPanel::offsetXChanged(double v)
{
    if (mUpdatingUI) { return; }
    if (LayerVideo* video = currentVideoLayer())
    {
        QPointF off = video->videoOffset();
        off.setX(v);
        video->setVideoOffset(off);
        editor()->getScribbleArea()->update();
    }
}

void VideoLayerPanel::offsetYChanged(double v)
{
    if (mUpdatingUI) { return; }
    if (LayerVideo* video = currentVideoLayer())
    {
        QPointF off = video->videoOffset();
        off.setY(v);
        video->setVideoOffset(off);
        editor()->getScribbleArea()->update();
    }
}

void VideoLayerPanel::scaleChanged(double percent)
{
    if (mUpdatingUI) { return; }
    if (LayerVideo* video = currentVideoLayer())
    {
        video->setVideoScale(percent / 100.0);
        editor()->getScribbleArea()->update();
    }
}
