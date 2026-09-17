#include "catch.hpp"

#include "editor.h"
#include "scribblearea.h"
#include "object.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "bitmapimage.h"
#include "undoredomanager.h"
#include "scriptapi.h"

#include <QDir>
#include <QFile>
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
