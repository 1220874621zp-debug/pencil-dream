/*
    Pencil Dream（Pencil2D TVP化分支）— 分镜面板
    分镜 = 图层结构的投影视图：连续组或未分组位图层 = 一个镜头；
    Action/Dialog/Notes 注释存于图层属性；拖动重排/并组走 LayerOrderCommand 事务。
*/
#ifndef STORYBOARDPANEL_H
#define STORYBOARDPANEL_H

#include "basedockwidget.h"

#include <QAbstractScrollArea>
#include <QHash>
#include <QPixmap>
#include <QQueue>
#include <QRect>
#include <QSet>
#include <QTimer>
#include <QVector>

class Editor;
class LayerCamera;
class QLabel;
class QPushButton;
class QSlider;

/** 分镜镜头（组镜头或散层镜头；全部为重建时的值拷贝，不持有 Layer*） */
struct StoryboardShot
{
    int groupId = -1;        // <0 = 散层镜头
    int firstLayerIndex = 0; // 栈序首成员（索引小 = 视觉低 = 先画在下）
    int layerCount = 0;
    int anchorLayerId = -1;  // 首成员层 id（注释三字段与缩略图锚点）
    int topLayerId = -1;     // 视觉最上成员（索引最大；点击选层用）
    QString name;
    int colorIndex = -1;
    bool visible = true;
    int firstFrame = 1;
    int lastFrame = 0;       // lastFrame < firstFrame = 无关键帧
    bool hasAction = false;
    bool hasDialog = false;
    bool hasNotes = false;
    int shotType = -1;        // 景别索引 0-6（-1 = 未设置；代号存锚点层 sbShotType）
    bool cameraMoves = false; // 镜头帧范围内相机关键帧 > 1
};

/** 缩略图缓存条目：worldRect 为位图世界坐标内容域（相机取景框叠加映射用） */
struct StoryboardThumb
{
    QPixmap pixmap;
    QRect worldRect;
    bool valid = false;
};

/** 分镜卡墙：单视口自绘 QAbstractScrollArea（模式同 ExposureSheetView；
 *  卡片折行网格 + 拖动重排/并组 + 异步多层合成缩略图） */
class StoryboardView : public QAbstractScrollArea
{
    Q_OBJECT
public:
    explicit StoryboardView(QWidget* parent = nullptr);

    void setEditor(Editor* editor) { mEditor = editor; }

    void rebuildShots();            // 层结构/组/帧数据变化：全量重建
    void updateCurrentFrame(int frame); // 播放头移动：仅重绘高亮
    void refreshHighlight();        // 当前层变化：同步选中卡

    void setCardWidth(int width);
    int cardWidth() const { return mCardW; }
    void setShowCamera(bool on) { mShowCamera = on; update(); }
    void setShowFrames(bool on) { mShowFrames = on; update(); }
    bool showFrames() const { return mShowFrames; }
    void setOrderLocked(bool locked) { mOrderLocked = locked; }
    void setFps(int fps) { mFps = qMax(1, fps); }
    int fps() const { return mFps; }
    void clearThumbCache() { mThumbCache.clear(); mThumbQueue.clear(); mThumbQueued.clear(); }

    void showDynamicsChart(); // 影像力学：景别-时间节奏曲线（面板工具栏入口）

    int shotCount() const { return mShots.size(); }
    int totalFrames() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    // 几何
    int  thumbH() const { return mCardW * 9 / 16; }
    int  infoH() const { return 52; }
    int  cardH() const { return thumbH() + infoH(); }
    int  columns() const;
    QRect cardRect(int shotIndex) const; // 内容坐标
    int   shotIndexAt(const QPoint& contentPos) const; // 卡片命中（-1 = 空白）
    void  eyeRectFor(int shotIndex, QRect* outEye) const;
    int   gapIndexAt(const QPoint& contentPos) const;  // 拖放插入槽位（0..N）
    void  updateScrollRanges();

    // 镜头操作
    void selectShot(int shotIndex);
    void toggleShotVisible(int shotIndex);
    void showEditDialog(int shotIndex);
    void showRenameDialog(int shotIndex);
    void playFromShot(int shotIndex);
    void applyShotColor(int shotIndex, int colorIndex);
    void applyShotType(int shotIndex, int typeIndex);
    void commitDrag();

    // 缩略图
    const StoryboardThumb* thumbnailFor(int shotIndex);
    void processThumbQueue();

    // 绘制
    void drawCard(QPainter& painter, int shotIndex, const QRect& rect);
    void drawCameraOverlay(QPainter& painter, const QRect& thumbRect,
                           const QRect& worldRect, int frame);
    QString durationText(const StoryboardShot& shot) const;

    Editor* mEditor = nullptr;

    QVector<StoryboardShot> mShots;
    int mCurrentFrame = 1;
    int mCurrentShotByLayer = -1; // 当前层所属镜头（高亮跟随）
    int mFps = 12;

    // 显示开关
    int  mCardW = 160;
    bool mShowFrames = true;  // true=帧 false=秒
    bool mShowCamera = false;
    bool mOrderLocked = false;

    // 悬浮
    int mHoverShot = -1;
    bool mHoverEye = false;

    // 拖动重排/并组
    bool   mDragArmed = false;
    bool   mDragging = false;
    QPoint mDragPressPos;
    int    mDragSourceShot = -1;
    QPoint mDragPos;        // 视口坐标
    int    mDragGap = -1;   // 插入槽位
    int    mDragMergeShot = -1; // 并组目标（-1 = 无）

    // 缩略图异步引擎（锚点层id<<32|帧号；同 TimeLineCells 模式）
    QHash<qint64, StoryboardThumb> mThumbCache;
    QQueue<QPair<int, int>> mThumbQueue; // (anchorLayerId, frame)
    QSet<qint64> mThumbQueued;
    QTimer* mThumbTimer = nullptr;

    int mCameraLayerId = -1;
};

/** 分镜面板：工具条（锁序/相机框/帧秒/尺寸）+ 卡墙 */
class StoryboardPanel : public BaseDockWidget
{
    Q_OBJECT
public:
    explicit StoryboardPanel(QWidget* parent = nullptr);

    void initUI() override;
    void updateUI() override;

private:
    StoryboardView* mView = nullptr;
    QPushButton* mLockButton = nullptr;
    QPushButton* mCameraButton = nullptr;
    QPushButton* mDynamicsButton = nullptr;
    QPushButton* mTimeModeButton = nullptr;
    QSlider* mSizeSlider = nullptr;
    QLabel* mStatusLabel = nullptr;

    void updateStatus();
};

#endif // STORYBOARDPANEL_H
