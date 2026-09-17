#include "catch.hpp"

#include "editor.h"
#include "scribblearea.h"
#include "object.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "bitmapimage.h"
#include "undoredomanager.h"
#include "selectionmanager.h"
#include "scriptapi.h"

#include <QDir>
#include <QFile>
#include <QLine>
#include <QPen>
#include <QTemporaryDir>

// 脚本系统端到端：QJSEngine 装载 → registerCommand → runCommand
// → keyFramePositions/keyFrameBounds/canvasRect 查询 → scaleKeyFrame 修改
// → 撤销组单步回滚（与「批量缩放关键帧适配画布」示例脚本同核心链路）
namespace
{

Editor* makeEditorWithTwoFrames(LayerBitmap** outLayer)
{
    Object* object = new Object;
    object->init();
    Editor* editor = new Editor;
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    editor->setScribbleArea(scribbleArea);
    editor->setObject(object);
    scribbleArea->setEditor(editor);
    editor->init();
    scribbleArea->init();

    LayerBitmap* layer = editor->layers()->createBitmapLayer("script");
    editor->layers()->setCurrentLayer(0);
    editor->scrubTo(1);

    QPen pen(QColor(255, 0, 0, 255));
    pen.setWidth(5);
    BitmapImage* f1 = static_cast<BitmapImage*>(layer->getKeyFrameAt(1));
    REQUIRE(f1 != nullptr);
    f1->drawLine(QPointF(-100, -50), QPointF(-40, -20), pen, QPainter::CompositionMode_SourceOver, false);

    editor->scrubTo(5);
    REQUIRE(layer->addNewKeyFrameAt(5));
    BitmapImage* f5 = static_cast<BitmapImage*>(layer->getKeyFrameAt(5));
    REQUIRE(f5 != nullptr);
    f5->drawLine(QPointF(-90, -60), QPointF(-30, -30), pen, QPainter::CompositionMode_SourceOver, false);

    if (outLayer) { *outLayer = layer; }
    return editor;
}

// 与示例脚本同核心：并集边框 → 统一比例缩放 → 并集中心对齐画布中心
const char* kScaleScript = R"JS(
registerCommand("测试缩放", function () {
    var idx = pencil.activeLayerIndex();
    var rect = pencil.canvasRect();
    var pos = pencil.keyFramePositions(idx);
    if (pos.length != 2) { throw "期望 2 个关键帧，得到 " + pos.length; }
    var u = null;
    for (var i = 0; i < pos.length; i++) {
        var b = pencil.keyFrameBounds(idx, pos[i]);
        if (!b || !b.width || !b.height) { continue; }
        if (u === null) {
            u = { x: b.x, y: b.y, w: b.width, h: b.height };
        } else {
            var x2 = Math.max(u.x + u.w, b.x + b.width);
            var y2 = Math.max(u.y + u.h, b.y + b.height);
            u.x = Math.min(u.x, b.x); u.y = Math.min(u.y, b.y);
            u.w = x2 - u.x; u.h = y2 - u.h;
        }
    }
    if (u === null) { throw "没有内容"; }
    log("union=" + u.w + "x" + u.h + "@" + u.x + "," + u.y +
        " canvas=" + rect.width + "x" + rect.height);
    pencil.beginUndoGroup("测试：脚本缩放");
    for (var i = 0; i < pos.length; i++) {
        if (!pencil.scaleKeyFrame(idx, pos[i], 2.0,
                                  u.x + u.w / 2, u.y + u.h / 2,
                                  rect.x + rect.width / 2, rect.y + rect.height / 2)) {
            throw "scaleKeyFrame 失败：" + pos[i];
        }
    }
    pencil.endUndoGroup();
});
)JS";

// 与「按实际像素裁剪关键帧」示例脚本同核心：每帧裁到自己内容边框，空帧跳过
const char* kCropScript = R"JS(
registerCommand("测试裁剪", function () {
    var idx = pencil.activeLayerIndex();
    var pos = pencil.keyFramePositions(idx);
    var done = 0, skipped = 0;
    pencil.beginUndoGroup("测试：脚本裁剪");
    for (var i = 0; i < pos.length; i++) {
        var b = pencil.keyFrameBounds(idx, pos[i]);
        if (!b || !b.width || !b.height) { skipped++; continue; }
        if (!pencil.cropKeyFrame(idx, pos[i], b.x, b.y, b.width, b.height)) {
            throw "cropKeyFrame 失败：" + pos[i];
        }
        done++;
    }
    pencil.endUndoGroup();
    log("done=" + done + " skipped=" + skipped);
});
)JS";

} // namespace

TEST_CASE("ScriptHost load and query API")
{
    LayerBitmap* layer = nullptr;
    Editor* editor = makeEditorWithTwoFrames(&layer);

    QTemporaryDir tmp;
    const QString jsPath = QDir(tmp.path()).filePath("query.js");
    QFile file(jsPath);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(kScaleScript);
    file.close();

    ScriptHost host(editor, nullptr);
    REQUIRE(host.loadScript(jsPath).isEmpty());
    REQUIRE(host.commandLabels() == QStringList{ QStringLiteral("测试缩放") });

    // 语法错误的脚本返回带行号的错误
    const QString badPath = QDir(tmp.path()).filePath("bad.js");
    QFile bad(badPath);
    REQUIRE(bad.open(QIODevice::WriteOnly));
    bad.write("function {{{");
    bad.close();
    ScriptHost badHost(editor, nullptr);
    REQUIRE_FALSE(badHost.loadScript(badPath).isEmpty());
    REQUIRE(badHost.commandLabels().isEmpty());

    delete editor;
}

TEST_CASE("ScriptHost scale all keyframes in one undo step")
{
    LayerBitmap* layer = nullptr;
    Editor* editor = makeEditorWithTwoFrames(&layer);

    auto* frame1 = static_cast<BitmapImage*>(layer->getKeyFrameAt(1));
    auto* frame5 = static_cast<BitmapImage*>(layer->getKeyFrameAt(5));
    const QRect bounds1Before = frame1->bounds();
    const QRect bounds5Before = frame5->bounds();
    REQUIRE_FALSE(bounds1Before.isEmpty());
    REQUIRE_FALSE(bounds5Before.isEmpty());

    QTemporaryDir tmp;
    const QString jsPath = QDir(tmp.path()).filePath("scale.js");
    QFile file(jsPath);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(kScaleScript);
    file.close();

    ScriptHost host(editor, nullptr);
    REQUIRE(host.loadScript(jsPath).isEmpty());

    // 运行前画布中心锚点已记录；运行后两个关键帧同比例放大并居中
    const QString runError = host.runCommand(QStringLiteral("测试缩放"));
    INFO(runError.toStdString());
    REQUIRE(runError.isEmpty());

    const QStringList output = host.takeOutput();
    REQUIRE(output.size() == 1); // union 日志恰好一条

    REQUIRE(host.touchedFrames().size() == 2);
    REQUIRE(frame1->bounds().width() == bounds1Before.width() * 2);
    REQUIRE(frame5->bounds().width() == bounds5Before.width() * 2);

    // 撤销组：一步回滚两个关键帧到原 bounds
    editor->undoRedo()->undo();
    REQUIRE(frame1->bounds() == bounds1Before);
    REQUIRE(frame5->bounds() == bounds5Before);

    // 重做：再次放大
    editor->undoRedo()->redo();
    REQUIRE(frame1->bounds().width() == bounds1Before.width() * 2);
    REQUIRE(frame5->bounds().width() == bounds5Before.width() * 2);

    delete editor;
}

TEST_CASE("ScriptHost crop all keyframes to content bounds")
{
    LayerBitmap* layer = nullptr;
    Editor* editor = makeEditorWithTwoFrames(&layer);
    auto* frame1 = static_cast<BitmapImage*>(layer->getKeyFrameAt(1));
    auto* frame5 = static_cast<BitmapImage*>(layer->getKeyFrameAt(5));

    // 用“带透明边距的图像 + mMinBound=true”整体替换两帧，模拟文件导入的线稿
    // （文件导入帧 mMinBound=true，bounds() 查询会跳过 autoCrop，透明边距得以保留；
    //  手画帧 mMinBound=false 一查 bounds() 就被自动裁掉，垫不出边距）
    // 注意 operator= 会连 KeyFrame 元数据（pos 等）一起替换，必须 setPos 回原位，
    // 否则 key->pos() 与层内 map 键脱节，脚本的 keyFramePositions 全是 -1
    auto makePadded = [](int keyPos, const QRect& imageRect, const QLine& lineInImage) -> BitmapImage
    {
        QImage img(imageRect.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        QPen pen(QColor(255, 0, 0, 255));
        pen.setWidth(5);
        p.setPen(pen);
        p.drawLine(lineInImage);
        p.end();
        // 四角撒微透明噪点（模拟转线稿背景噪声，alpha=3 < 默认阈值 8）
        img.setPixel(0, 0, qPremultiply(qRgba(255, 255, 255, 3)));
        img.setPixel(img.width() - 1, 0, qPremultiply(qRgba(255, 255, 255, 3)));
        img.setPixel(0, img.height() - 1, qPremultiply(qRgba(255, 255, 255, 3)));
        img.setPixel(img.width() - 1, img.height() - 1, qPremultiply(qRgba(255, 255, 255, 3)));
        BitmapImage frame(imageRect.topLeft(), img);
        frame.setPos(keyPos);
        frame.enableAutoCrop(true); // 与 LayerBitmap::createKeyFrame 创建的帧一致
        return frame;
    };
    *frame1 = makePadded(1, QRect(-500, -400, 1000, 800), QLine(397, 347, 463, 383)); // 线全局(-103,-53)~(-37,-17)
    *frame5 = makePadded(5, QRect(-600, -500, 1200, 1000), QLine(510, 440, 570, 470)); // 线全局(-90,-60)~(-30,-30)
    REQUIRE(frame1->bounds() == QRect(-500, -400, 1000, 800));
    REQUIRE(frame5->bounds() == QRect(-600, -500, 1200, 1000));
    const QRgb sampleBefore = frame1->constScanLine(-100, -50);
    REQUIRE(sampleBefore != 0);

    // 第三个关键帧为空：应被跳过而非报错
    editor->scrubTo(10);
    REQUIRE(layer->addNewKeyFrameAt(10));

    // 建立旧选区（变形/移动工具的变换框寄生其上）：帧1 的大框
    editor->scrubTo(1);
    editor->selectAll();
    REQUIRE(editor->select()->mySelectionRect() == QRectF(-500, -400, 1000, 800));

    // 噪声阈值：默认 8 滤掉四角 alpha=3 噪点（边框=线框）；minAlpha=0 时噪点撑满全图
    ScriptHost probeHost(editor, nullptr);
    const int lineW = probeHost.keyFrameBounds(0, 1).value(QStringLiteral("width")).toInt();
    REQUIRE(lineW > 0);
    REQUIRE(lineW < 100);
    const QVariantMap noisy = probeHost.keyFrameBounds(0, 1, 0);
    REQUIRE(noisy.value(QStringLiteral("width")).toInt() == 1000);
    REQUIRE(noisy.value(QStringLiteral("height")).toInt() == 800);

    QTemporaryDir tmp;
    const QString jsPath = QDir(tmp.path()).filePath("crop.js");
    QFile file(jsPath);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(kCropScript);
    file.close();

    ScriptHost host(editor, nullptr);
    REQUIRE(host.loadScript(jsPath).isEmpty());

    const QString runError = host.runCommand(QStringLiteral("测试裁剪"));
    INFO(runError.toStdString());
    REQUIRE(runError.isEmpty());

    // 两帧各自收紧到内容边框（图像 bounds == 内容实际边框）
    const QVariantMap cb1 = host.keyFrameBounds(0, 1);
    REQUIRE(cb1.value(QStringLiteral("width")).toInt() > 0);
    const QRect expected1(cb1.value(QStringLiteral("x")).toInt(),
                          cb1.value(QStringLiteral("y")).toInt(),
                          cb1.value(QStringLiteral("width")).toInt(),
                          cb1.value(QStringLiteral("height")).toInt());
    REQUIRE(frame1->bounds() == expected1);
    REQUIRE(frame1->bounds().width() < 1000);
    REQUIRE(static_cast<BitmapImage*>(layer->getKeyFrameAt(5))->bounds().width() < 1200);

    // 内容像素原位保留（画布显示不变），空帧未被触碰
    REQUIRE(frame1->constScanLine(-100, -50) == sampleBefore);
    REQUIRE(static_cast<BitmapImage*>(layer->getKeyFrameAt(10))->bounds().isEmpty());

    // 选区失效：脚本改帧后旧大框被清除；再次全选按新内容边框重建
    REQUIRE(editor->select()->mySelectionRect().isNull());
    editor->selectAll();
    REQUIRE(editor->select()->mySelectionRect() == QRectF(expected1));

    // 撤销组：一步回滚两个关键帧到带透明边距的状态
    editor->undoRedo()->undo();
    REQUIRE(frame1->bounds() == QRect(-500, -400, 1000, 800));
    REQUIRE(frame5->bounds() == QRect(-600, -500, 1200, 1000));
    REQUIRE(frame1->constScanLine(-100, -50) == sampleBefore);

    delete editor;
}

TEST_CASE("ScriptHost runCommand reports JS exceptions")
{
    LayerBitmap* layer = nullptr;
    Editor* editor = makeEditorWithTwoFrames(&layer);

    QTemporaryDir tmp;
    const QString jsPath = QDir(tmp.path()).filePath("throw.js");
    QFile file(jsPath);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write("registerCommand(\"boom\", function () { pencil.log(\"前置输出\"); throw \"爆炸\"; });");
    file.close();

    ScriptHost host(editor, nullptr);
    REQUIRE(host.loadScript(jsPath).isEmpty());

    const QString error = host.runCommand(QStringLiteral("boom"));
    REQUIRE_FALSE(error.isEmpty());
    REQUIRE(error.contains(QStringLiteral("爆炸")));

    // 异常前的 log 仍可取走；未知命令报错
    REQUIRE(host.takeOutput() == QStringList{ QStringLiteral("前置输出") });
    REQUIRE_FALSE(host.runCommand(QStringLiteral("不存在")).isEmpty());

    delete editor;
}
