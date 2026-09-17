/*
 * Pencil Dream - JS 脚本宿主
 * 脚本系统：语言选型与 friction 一致（QJSEngine / ECMAScript，Qt 自带零额外部署）。
 * 每个脚本文件一个 ScriptHost（引擎隔离）；脚本顶层可用：
 *   registerCommand(label, fn)  注册菜单命令
 *   log(msg) / alert(msg) / confirm(msg)
 * 全局 `pencil` 对象即本类的 Q_INVOKABLE 方法集。
 */

#ifndef SCRIPTAPI_H
#define SCRIPTAPI_H

#include <QObject>
#include <QJSValue>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

class QJSEngine;
class Editor;
class QWidget;
class QUndoCommand;
class BitmapImage;

class ScriptHost : public QObject
{
    Q_OBJECT

public:
    explicit ScriptHost(Editor* editor, QWidget* dialogParent, QObject* parent = nullptr);
    ~ScriptHost() override;

    /** 读取（UTF-8）并执行脚本文件。载入即执行脚本体；返回空串=成功，否则为错误描述。 */
    QString loadScript(const QString& filePath);

    /** 本脚本注册的命令标签（按注册顺序）。 */
    QStringList commandLabels() const
    {
        QStringList labels;
        for (const auto& entry : mCommands) { labels << entry.first; }
        return labels;
    }

    /** 调用一条已注册命令。返回空串=成功，否则为错误描述。 */
    QString runCommand(const QString& label);

    /** 取走运行期 log() 输出（清空缓冲）。 */
    QStringList takeOutput();

    /** 脚本异常退出后强制收口未闭合的撤销组（安全网，正常运行无需调用）。 */
    void forceCloseUndoGroup();

    /** 本脚本是否修改过任何帧（显示缓存失效依据）。 */
    QList<int> touchedFrames() const { return mTouchedFrames; }
    void clearTouchedFrames() { mTouchedFrames.clear(); }

    // ---- JS API：全局对象 `pencil` ----

    Q_INVOKABLE QString version() const;

    Q_INVOKABLE int currentFrame() const;
    Q_INVOKABLE int fps() const;

    // 图层
    Q_INVOKABLE int layerCount() const;
    Q_INVOKABLE QString layerName(int layerIndex) const;
    /** bitmap / vector / movie / sound / camera / colorize / undefined */
    Q_INVOKABLE QString layerType(int layerIndex) const;
    Q_INVOKABLE bool layerVisible(int layerIndex) const;
    Q_INVOKABLE int activeLayerIndex() const;
    Q_INVOKABLE bool setActiveLayer(int layerIndex);

    // 关键帧（真实关键帧位，非循环层显示帧）
    Q_INVOKABLE QVariantList keyFramePositions(int layerIndex) const;

    /** 画布（相机）尺寸 {width,height}；无相机层时回退 1920x1080 */
    Q_INVOKABLE QVariantMap canvasSize() const;

    /** 画布矩形 {x,y,width,height}（图层坐标系）。注意 pencil 原点在画布中心，
     *  通常 x=-width/2；脚本要把内容定位到画布中心请用 rect.x + rect.width/2。 */
    Q_INVOKABLE QVariantMap canvasRect() const;

    /** 关键帧内容实际包围盒（非透明像素）{x,y,width,height}，画布全局坐标；
     *  空帧或非位图关键帧返回 undefined（JS 侧 null）。 */
    Q_INVOKABLE QVariantMap keyFrameBounds(int layerIndex, int pos) const;

    /**
     * 等比缩放一个位图关键帧的整幅图像，并平移使内容锚点 (anchorX,anchorY)
     * 落到 (dstX,dstY)。修改进撤销栈；处于 beginUndoGroup/endUndoGroup
     * 之间时并入同一步撤销。
     */
    Q_INVOKABLE bool scaleKeyFrame(int layerIndex, int pos, double scale,
                                   double anchorX, double anchorY,
                                   double dstX, double dstY);

    /**
     * 把一个位图关键帧的图像裁剪到全局矩形 (x,y,w,h)（画布坐标系，
     * 与 keyFrameBounds 的返回值直接配套）。内容像素位置不变，
     * 仅收紧图像边界与尺寸（去透明边距）。修改进撤销栈（同撤销组规则）。
     */
    Q_INVOKABLE bool cropKeyFrame(int layerIndex, int pos,
                                  double x, double y, double width, double height);

    // 撤销组：组内所有修改并入单步撤销（Ctrl+Z 一次回滚）
    Q_INVOKABLE bool beginUndoGroup(const QString& label);
    Q_INVOKABLE bool endUndoGroup();

    // ---- JS API：顶层胶水函数的后端 ----
    Q_INVOKABLE void registerCommand(const QString& label, const QJSValue& fn);
    Q_INVOKABLE void log(const QString& message);
    Q_INVOKABLE void alertBox(const QString& message);
    Q_INVOKABLE bool confirmBox(const QString& message);

private:
    void installApi();

    /** 位图关键帧修改骨架：校验+加载+双快照+mutator+撤销命令（组内并入/组外单步）
     *  +数据失效与 touchedFrames 记录。mutator 直接改传入的 BitmapImage 内容。 */
    bool modifyKeyFrameWithUndo(int layerIndex, int pos, const QString& undoText,
                                const std::function<void(BitmapImage*)>& mutate);

    Editor* mEditor = nullptr;
    QWidget* mDialogParent = nullptr;
    QJSEngine* mEngine = nullptr;

    QList<QPair<QString, QJSValue>> mCommands; // label → 回调（保注册顺序，允许同名）
    QStringList mOutputBuffer;
    QList<int> mTouchedFrames;
    QUndoCommand* mUndoMacro = nullptr;        // 未闭合的撤销组（空壳宏命令）
    QString mUndoMacroLabel;
};

#endif // SCRIPTAPI_H
