// Pencil Dream 内置脚本：按实际图像像素裁剪关键帧
//
// 场景：图片（如扫描稿/照片）经「滤镜 → 颜色转为透明度」变成线稿后，
// 四周留下大片透明边距，图像尺寸仍是原图大小。本脚本把图层上每个关键帧
// 按画布上的实际内容像素计算边框，裁剪成该边框的宽高。
//
// 特性：
//   1. 每帧各自裁剪到自己的紧致内容边框（去掉透明边距，尺寸最小化）；
//   2. 内容像素在画布上的位置不变（动画对位、画布显示不受影响）；
//   3. 空帧（无任何有效内容像素）自动跳过；
//   4. 整个操作合并为一步撤销（Ctrl+Z 一次回滚）。
//
// 脚本静默执行：结果进 log（菜单「脚本 → 查看脚本输出」回看），出错才弹窗。
// 本文件为内置脚本，升级时会被程序覆盖释放；要定制请复制改名后修改，
// 再用菜单「脚本 → 重新载入脚本」生效。
//
// ——校准方法（重要）——
// 如果裁出来的边框还是接近原图大小，说明内容周围有转线稿留下的低不透明度
// 灰雾（alpha 几十的像素铺满画面）。把 MIN_ALPHA 往上调（64 → 96 → 128 …）
// 直到边框收进线稿主体；调完「重新载入脚本」即生效，无须重启。
// 不确定时先设 DRY_RUN = true 试运行：只打印每帧将裁成的边框，不动数据，
// 看数字合适后改回 false 真正裁剪。

var MIN_ALPHA = 64; // 低于该 alpha 的像素视为透明噪声（0 = 严格按非零判定）
var DRY_RUN = false; // true = 只打印边框不实际裁剪（校准用）

registerCommand("按实际像素裁剪关键帧", function () {
    var idx = pencil.activeLayerIndex();
    if (idx < 0) {
        alert("没有活动图层，请先选中一个位图图层。");
        return;
    }
    var type = pencil.layerType(idx);
    if (type !== "bitmap" && type !== "colorize") {
        alert("请在位图（或填色）图层上运行，当前图层类型：" + type);
        return;
    }
    log("图层：" + pencil.layerName(idx) + "，噪声阈值 alpha≥" + MIN_ALPHA
        + (DRY_RUN ? "（试运行，不实际裁剪）" : ""));

    var positions = pencil.keyFramePositions(idx);
    if (positions.length === 0) {
        alert("当前图层没有关键帧。");
        return;
    }

    if (!DRY_RUN) { pencil.beginUndoGroup("脚本：按实际像素裁剪关键帧"); }
    pencil.progressBegin(positions.length, "正在按实际像素裁剪关键帧…");
    var done = 0, skipped = 0, canceled = false;
    var minWidth = 0, minHeight = 0, maxWidth = 0, maxHeight = 0;
    for (var i = 0; i < positions.length; i++) {
        if (!pencil.progressSetValue(i)) { canceled = true; break; }
        var pos = positions[i];
        var b = pencil.keyFrameBounds(idx, pos, MIN_ALPHA);
        if (!b || !b.width || !b.height || b.width <= 0 || b.height <= 0) {
            skipped++;
            log("第 " + pos + " 帧：空帧，跳过");
            continue;
        }
        if (DRY_RUN) {
            done++;
            log("第 " + pos + " 帧：将裁剪为 " + b.width + "×" + b.height + "@(" + b.x + "," + b.y + ")");
        } else if (pencil.cropKeyFrame(idx, pos, b.x, b.y, b.width, b.height)) {
            done++;
            log("第 " + pos + " 帧：裁剪为 " + b.width + "×" + b.height + "@(" + b.x + "," + b.y + ")");
        }
        if (done > 0) {
            if (b.width > maxWidth) maxWidth = b.width;
            if (b.height > maxHeight) maxHeight = b.height;
            if (minWidth === 0 || b.width < minWidth) minWidth = b.width;
            if (minHeight === 0 || b.height < minHeight) minHeight = b.height;
        }
    }
    pencil.progressEnd();
    if (!DRY_RUN) { pencil.endUndoGroup(); }

    if (done === 0) {
        log("没有可裁剪的帧（所有关键帧都是空的）。");
        return;
    }
    log((DRY_RUN ? "试运行：共 " : "共裁剪 ") + done + " 帧，帧尺寸范围 "
        + minWidth + "~" + maxWidth + " × " + minHeight + "~" + maxHeight
        + (skipped > 0 ? "，跳过空帧 " + skipped + " 个" : "")
        + (canceled ? "。已取消，已处理的帧可 Ctrl+Z 撤销。"
                    : (DRY_RUN ? "。数字合适后把 DRY_RUN 改回 false 再跑。"
                               : "，内容位置不变（Ctrl+Z 可一次撤销）。")));
});
