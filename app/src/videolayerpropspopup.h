#ifndef VIDEOLAYERPROPSPOPUP_H
#define VIDEOLAYERPROPSPOPUP_H

#include <QWidget>

class QSlider;
class QLineEdit;
class LayerVideo;
class Editor;

// AE 式可拖动数值段:横向拖动改值(Shift 加速),双击进入精确输入。
// hover 光标变 ↔(可拖交互必须配光标预览,否则用户盲点)。
class DragNumberField : public QWidget
{
    Q_OBJECT

public:
    DragNumberField(double min, double max, double step, int decimals,
                    const QString& suffix, QWidget* parent);

    double value() const { return mValue; }
    void setValue(double v); // 外部同步(钳制+发信号;回填方用守卫挡回环)

signals:
    void valueEdited(double);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    void startEditing();
    void finishEditing();

    double mMin = 0.0, mMax = 100.0, mStep = 1.0;
    int mDecimals = 1;
    QString mSuffix;
    double mValue = 0.0;
    double mPressValue = 0.0;
    bool mDragging = false;
    bool mHovered = false;
    QPoint mPressPos;
    QLineEdit* mEditor = nullptr;
};

// 图层行小三角拉出的参考视频属性面板:缩放(滑杆+拖动数值)+位移X/Y(拖动数值)。
// Qt::Popup 语义:点外部即关并自毁(hideEvent→deleteLater);打开期间
// 拦截外部交互,持有的层指针不会被并发删除。
class VideoLayerPropsPopup : public QWidget
{
    Q_OBJECT

public:
    static void showPopup(LayerVideo* layer, Editor* editor, const QPoint& globalPos, QWidget* parent);

protected:
    void hideEvent(QHideEvent*) override;

private slots:
    void offsetFieldEdited(int axis, double v);
    void scaleFieldEdited(double v);
    void scaleSliderMoved(int v);

private:
    VideoLayerPropsPopup(LayerVideo* layer, Editor* editor, QWidget* parent);
    void applyToLayer();

    LayerVideo* mLayer = nullptr;
    Editor* mEditor = nullptr;
    QSlider* mScaleSlider = nullptr;
    DragNumberField* mScaleField = nullptr;
    DragNumberField* mOffsetXField = nullptr;
    DragNumberField* mOffsetYField = nullptr;
    bool mSyncing = false; // 滑杆<->输入框双向同步防环
};

#endif // VIDEOLAYERPROPSPOPUP_H
