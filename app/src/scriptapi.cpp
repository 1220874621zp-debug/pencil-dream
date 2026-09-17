/*
 * Pencil Dream - JS 脚本宿主实现
 */

#include "scriptapi.h"

#include <QJSEngine>
#include <QQmlEngine>
#include <QFile>
#include <QMessageBox>
#include <QTransform>
#include <QtMath>

#include <climits>

#include "editor.h"
#include "layer.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "object.h"
#include "undoredocommand.h"
#include "undoredomanager.h"

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

namespace
{

/** 画布尺寸：取第一个相机层的视图尺寸，无相机层时回退 1920x1080（与导出器口径一致） */
QSize cameraViewSize(Editor* editor)
{
    const auto cameras = editor->object()->getLayersByType<LayerCamera>();
    if (!cameras.empty())
    {
        return cameras.front()->getViewSize();
    }
    return QSize(1920, 1080);
}

/** 空壳撤销宏：刻意不重写 undo()/redo()，使用 QUndoCommand 默认实现
 *  （push 时正序调子命令 redo、撤销时逆序调子命令 undo）。
 *  子命令（BitmapReplaceCommand 等）在构造时以本宏为 parent。 */
class UndoGroupCommand : public QUndoCommand
{
public:
    explicit UndoGroupCommand(const QString& text) : QUndoCommand(text) {}
};

QString layerTypeToString(Layer::LAYER_TYPE type)
{
    switch (type)
    {
    case Layer::BITMAP:    return QStringLiteral("bitmap");
    case Layer::VECTOR:    return QStringLiteral("vector");
    case Layer::MOVIE:     return QStringLiteral("movie");
    case Layer::SOUND:     return QStringLiteral("sound");
    case Layer::CAMERA:    return QStringLiteral("camera");
    case Layer::COLORIZE:  return QStringLiteral("colorize");
    default:               return QStringLiteral("undefined");
    }
}

/** 内容实际包围盒（非透明像素），图像局部坐标；空图返回空矩形 */
QRect contentBounds(const QImage& image)
{
    int minX = INT_MAX, minY = INT_MAX, maxX = -1, maxY = -1;
    const int w = image.width(), h = image.height();
    for (int y = 0; y < h; ++y)
    {
        const QRgb* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < w; ++x)
        {
            if (qAlpha(line[x]) != 0)
            {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }
    if (maxX < 0) { return QRect(); }
    return QRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
}

} // namespace

ScriptHost::ScriptHost(Editor* editor, QWidget* dialogParent, QObject* parent)
    : QObject(parent)
    , mEditor(editor)
    , mDialogParent(dialogParent)
    , mEngine(new QJSEngine)
{
    installApi();
}

ScriptHost::~ScriptHost()
{
    // QJSValue 不得比引擎活得久：先释放 JS 回调，再删引擎
    mCommands.clear();
    // 未闭合的撤销组里只有已构造未入栈的子命令，整树删除即可
    delete mUndoMacro;
    delete mEngine;
}

void ScriptHost::installApi()
{
    // CppOwnership：宿主生命周期归 C++ 侧（ScriptManager），防止 JS GC 收走
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
    const QJSValue hostObj = mEngine->newQObject(this);
    mEngine->globalObject().setProperty(QStringLiteral("pencil"), hostObj);

    // 顶层胶水：让脚本写 registerCommand(...) / log(...) / alert(...) / confirm(...)
    // __pencilSafeCall：JS 侧 try/catch 包装——QJSValue::isError() 只认 Error 对象，
    // 脚本 throw 原始值（字符串等）也要能报到 C++ 侧
    const QJSValue result = mEngine->evaluate(QStringLiteral(
        "function registerCommand(label, fn) { pencil.registerCommand(label, fn); }\n"
        "function log(msg) { pencil.log(String(msg)); }\n"
        "function alert(msg) { pencil.alertBox(String(msg)); }\n"
        "function confirm(msg) { return pencil.confirmBox(String(msg)); }\n"
        "function __pencilSafeCall(fn) {\n"
        "    try { fn(); return null; }\n"
        "    catch (e) { return String(e) + (e && e.stack ? '\\n' + e.stack : ''); }\n"
        "}\n"));
    Q_ASSERT(!result.isError());
}

QString ScriptHost::loadScript(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return QStringLiteral("无法打开脚本文件：%1").arg(filePath);
    }
    const QString source = QString::fromUtf8(file.readAll());
    file.close();

    const QJSValue result = mEngine->evaluate(source, filePath);
    if (result.isError())
    {
        return QStringLiteral("载入脚本出错 第%1行：%2")
                .arg(result.property(QStringLiteral("lineNumber")).toInt())
                .arg(result.toString());
    }
    return QString();
}

QString ScriptHost::runCommand(const QString& label)
{
    for (const auto& entry : mCommands)
    {
        if (entry.first == label)
        {
            const QJSValue wrapper = mEngine->globalObject().property(QStringLiteral("__pencilSafeCall"));
            const QJSValue result = wrapper.call(QJSValueList{ entry.second });
            if (!result.isNull())
            {
                // 异常时命令可能已改数据：撤销组强制收口，保证已做修改仍可一次撤销
                forceCloseUndoGroup();
                return QStringLiteral("脚本出错：%1").arg(result.toString());
            }
            return QString();
        }
    }
    return QStringLiteral("找不到命令：%1").arg(label);
}

void ScriptHost::forceCloseUndoGroup()
{
    if (mUndoMacro != nullptr)
    {
        endUndoGroup();
    }
}

QStringList ScriptHost::takeOutput()
{
    const QStringList out = mOutputBuffer;
    mOutputBuffer.clear();
    return out;
}

// ---- 查询 API ----

QString ScriptHost::version() const
{
    return QStringLiteral(APP_VERSION);
}

int ScriptHost::currentFrame() const
{
    return mEditor->currentFrame();
}

int ScriptHost::fps() const
{
    return mEditor->fps();
}

int ScriptHost::layerCount() const
{
    return mEditor->object()->getLayerCount();
}

QString ScriptHost::layerName(int layerIndex) const
{
    Layer* layer = mEditor->object()->getLayer(layerIndex);
    return layer != nullptr ? layer->name() : QString();
}

QString ScriptHost::layerType(int layerIndex) const
{
    Layer* layer = mEditor->object()->getLayer(layerIndex);
    return layer != nullptr ? layerTypeToString(layer->type()) : QStringLiteral("undefined");
}

bool ScriptHost::layerVisible(int layerIndex) const
{
    Layer* layer = mEditor->object()->getLayer(layerIndex);
    return layer != nullptr ? layer->visible() : false;
}

int ScriptHost::activeLayerIndex() const
{
    return mEditor->currentLayerIndex();
}

bool ScriptHost::setActiveLayer(int layerIndex)
{
    if (mEditor->object()->getLayer(layerIndex) == nullptr) { return false; }
    mEditor->setCurrentLayerIndex(layerIndex);
    return true;
}

QVariantList ScriptHost::keyFramePositions(int layerIndex) const
{
    QVariantList positions;
    Layer* layer = mEditor->object()->getLayer(layerIndex);
    if (layer == nullptr) { return positions; }
    layer->foreachKeyFrame([&positions](KeyFrame* key)
    {
        positions.append(key->pos());
    });
    return positions;
}

QVariantMap ScriptHost::canvasSize() const
{
    const QSize size = cameraViewSize(mEditor);
    QVariantMap map;
    map.insert(QStringLiteral("width"), size.width());
    map.insert(QStringLiteral("height"), size.height());
    return map;
}

QVariantMap ScriptHost::canvasRect() const
{
    const QSize size = cameraViewSize(mEditor);
    QVariantMap map;
    map.insert(QStringLiteral("x"), -size.width() / 2);
    map.insert(QStringLiteral("y"), -size.height() / 2);
    map.insert(QStringLiteral("width"), size.width());
    map.insert(QStringLiteral("height"), size.height());
    return map;
}

QVariantMap ScriptHost::keyFrameBounds(int layerIndex, int pos) const
{
    QVariantMap map;
    Layer* layer = mEditor->object()->getLayer(layerIndex);
    if (layer == nullptr || !layer->isBitmapKind()) { return map; }

    auto* bitmapLayer = static_cast<LayerBitmap*>(layer);
    BitmapImage* bitmap = bitmapLayer->getBitmapImageAtFrame(pos);
    if (bitmap == nullptr) { return map; }
    bitmap->loadFile();

    const QImage* image = bitmap->image();
    if (image == nullptr || image->isNull()) { return map; }

    const QRect local = contentBounds(*image);
    if (local.isEmpty()) { return map; }

    // 图像局部坐标 → 画布全局坐标（图像 topLeft 来自 bounds，可能含历史偏移）
    const QPoint topLeft = bitmap->bounds().topLeft();
    map.insert(QStringLiteral("x"), topLeft.x() + local.x());
    map.insert(QStringLiteral("y"), topLeft.y() + local.y());
    map.insert(QStringLiteral("width"), local.width());
    map.insert(QStringLiteral("height"), local.height());
    return map;
}

// ---- 修改 API ----

bool ScriptHost::modifyKeyFrameWithUndo(int layerIndex, int pos, const QString& undoText,
                                        const std::function<void(BitmapImage*)>& mutate)
{
    Layer* layer = mEditor->object()->getLayer(layerIndex);
    if (layer == nullptr || !layer->isBitmapKind()) { return false; }

    auto* bitmapLayer = static_cast<LayerBitmap*>(layer);
    BitmapImage* bitmap = bitmapLayer->getBitmapImageAtFrame(pos);
    if (bitmap == nullptr) { return false; }
    bitmap->loadFile();

    const QImage* image = bitmap->image();
    if (image == nullptr || image->isNull()) { return false; }
    if (bitmap->bounds().isEmpty()) { return false; }

    // 撤销：显式双快照（操作任意关键帧，不经“当前帧”快照链）
    const BitmapImage undoSnapshot = *bitmap;

    mutate(bitmap);

    const BitmapImage redoSnapshot = *bitmap;
    auto* command = new BitmapReplaceCommand(&undoSnapshot, &redoSnapshot, layer->id(),
                                             undoText, mEditor, mUndoMacro);
    if (mUndoMacro == nullptr)
    {
        mEditor->undoRedo()->pushUndoCommand(command);
    }

    // 数据失效：传实际关键帧 pos（auto 块中部传显示帧找不到关键帧）
    mEditor->setModified(layerIndex, pos);
    if (!mTouchedFrames.contains(pos))
    {
        mTouchedFrames.append(pos);
    }

    // 选区失效：变形/移动工具的变换框寄生在 SelectionManager 的选区上，
    // 改了帧内容后旧矩形与新图像不再对应（裁剪后仍显示原图尺寸的框）——
    // 清除选区，用户下次全选（Ctrl+A）时按新内容边框重建
    mEditor->deselectAll();
    return true;
}

bool ScriptHost::scaleKeyFrame(int layerIndex, int pos, double scale,
                               double anchorX, double anchorY,
                               double dstX, double dstY)
{
    if (!qIsFinite(scale) || scale <= 0) { return false; }

    // 整幅等比缩放（平滑插值），再定位到目标锚点：
    // 期望 topLeft = dst − (anchor − 原 topLeft) × scale
    return modifyKeyFrameWithUndo(layerIndex, pos,
                                  mUndoMacroLabel.isEmpty()
                                      ? QStringLiteral("脚本：缩放关键帧")
                                      : mUndoMacroLabel,
                                  [scale, anchorX, anchorY, dstX, dstY](BitmapImage* bitmap)
    {
        const QRect bounds = bitmap->bounds();
        BitmapImage scaled = bitmap->transformed(bounds, QTransform::fromScale(scale, scale), true);
        const QPointF wantTopLeft = QPointF(dstX, dstY)
                - QPointF(anchorX - bounds.left(), anchorY - bounds.top()) * scale;
        scaled.moveTopLeft(wantTopLeft.toPoint());

        bitmap->clear();
        bitmap->paste(&scaled, QPainter::CompositionMode_SourceOver);
    });
}

bool ScriptHost::cropKeyFrame(int layerIndex, int pos,
                              double x, double y, double width, double height)
{
    if (!qIsFinite(x) || !qIsFinite(y) || !qIsFinite(width) || !qIsFinite(height)) { return false; }
    const QRect wantedRect(QPoint(qRound(x), qRound(y)), QSize(qRound(width), qRound(height)));
    if (wantedRect.isEmpty()) { return false; }

    // copy() 的 topLeft 保留全局坐标，clear+paste 回贴 → 内容像素位置不变，仅收紧边界
    return modifyKeyFrameWithUndo(layerIndex, pos,
                                  mUndoMacroLabel.isEmpty()
                                      ? QStringLiteral("脚本：裁剪关键帧")
                                      : mUndoMacroLabel,
                                  [wantedRect](BitmapImage* bitmap)
    {
        BitmapImage cropped = bitmap->copy(wantedRect);
        bitmap->clear();
        bitmap->paste(&cropped, QPainter::CompositionMode_SourceOver);
    });
}

bool ScriptHost::beginUndoGroup(const QString& label)
{
    if (mUndoMacro != nullptr) { return false; } // 不支持嵌套
    mUndoMacro = new UndoGroupCommand(label.isEmpty() ? QStringLiteral("脚本操作") : label);
    mUndoMacroLabel = mUndoMacro->text();
    return true;
}

bool ScriptHost::endUndoGroup()
{
    if (mUndoMacro == nullptr) { return false; }
    QUndoCommand* macro = mUndoMacro;
    mUndoMacro = nullptr;
    mUndoMacroLabel.clear();
    if (macro->childCount() == 0)
    {
        delete macro; // 空组不入栈（避免产生无操作的撤销步）
        return true;
    }
    mEditor->undoRedo()->pushUndoCommand(macro);
    return true;
}

// ---- 顶层胶水后端 ----

void ScriptHost::registerCommand(const QString& label, const QJSValue& fn)
{
    if (!fn.isCallable() || label.trimmed().isEmpty()) { return; }
    mCommands.append({ label.trimmed(), fn });
}

void ScriptHost::log(const QString& message)
{
    mOutputBuffer.append(message);
}

void ScriptHost::alertBox(const QString& message)
{
    QMessageBox::information(mDialogParent, tr("脚本"), message);
}

bool ScriptHost::confirmBox(const QString& message)
{
    return QMessageBox::question(mDialogParent, tr("脚本"), message,
                                 QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}
