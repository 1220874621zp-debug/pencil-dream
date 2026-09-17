// Pencil Dream 内置脚本：批量缩放图层所有关键帧（按图形边框自动匹配画布宽或高）
//
// 用法：选中一个位图（或填色）图层 → 菜单「脚本 → 批量缩放关键帧适配画布」。
// 脚本静默执行：结果进 log（菜单「脚本 → 查看脚本输出」回看），出错才弹窗。
//
// 逻辑：
//   1. 计算该图层所有关键帧内容边框（非透明像素包围盒）的并集；
//   2. 按并集宽高比与画布宽高比自动决定匹配宽或高：
//        MODE = "fit"  → 完整放入画布（默认，内容不超出画布）；
//        MODE = "fill" → 铺满画布（超出的部分被画布裁掉）；
//   3. 全部关键帧用同一比例等比缩放（帧间不抖动），并集中心对齐画布中心；
//   4. 整个操作合并为一步撤销（Ctrl+Z 一次回滚）。
//
// 本文件为内置脚本，升级时会被程序覆盖释放；要定制请复制改名后修改，
// 再用菜单「脚本 → 重新载入脚本」生效。

var MODE = "fit";      // "fit" = 完整放入画布；"fill" = 铺满画布（超出被裁）
var MIN_ALPHA = 64;    // 低于该 alpha 的像素视为透明噪声（转线稿背景灰雾）

registerCommand("批量缩放关键帧适配画布", function () {
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
    log("图层：" + pencil.layerName(idx));

    var positions = pencil.keyFramePositions(idx);
    if (positions.length === 0) {
        alert("当前图层没有关键帧。");
        return;
    }

    // 所有关键帧内容边框的并集（统一比例，帧间不抖动）
    // 注：keyFrameBounds 对空帧/无内容帧返回空，JS 侧表现为无 width 字段
    // 进度分两段：先扫描边框（每帧一次），再逐帧缩放
    pencil.progressBegin(positions.length * 2, "正在计算内容边框…");
    var ux = 0, uy = 0, uw = 0, uh = 0, hasContent = false, canceled = false;
    for (var i = 0; i < positions.length; i++) {
        if (!pencil.progressSetValue(i)) { canceled = true; break; }
        var b = pencil.keyFrameBounds(idx, positions[i], MIN_ALPHA);
        if (!b || !b.width || !b.height || b.width <= 0 || b.height <= 0) { continue; }
        if (!hasContent) {
            ux = b.x; uy = b.y; uw = b.width; uh = b.height;
            hasContent = true;
        } else {
            var x2 = Math.max(ux + uw, b.x + b.width);
            var y2 = Math.max(uy + uh, b.y + b.height);
            ux = Math.min(ux, b.x); uy = Math.min(uy, b.y);
            uw = x2 - ux; uh = y2 - uy;
        }
    }
    if (!hasContent) {
        pencil.progressEnd();
        alert("该图层所有关键帧都是空的，没有可缩放的内容。");
        return;
    }
    if (canceled) {
        pencil.progressEnd();
        log("已取消。");
        return;
    }

    var rect = pencil.canvasRect(); // 画布矩形（pencil 原点在画布中心，rect.x 为负）
    var sx = rect.width / uw;
    var sy = rect.height / uh;
    var fill = (MODE === "fill");
    var scale = fill ? Math.max(sx, sy) : Math.min(sx, sy);
    // 自动判断匹配的是画布宽还是画布高
    var edge = fill ? (sx >= sy ? "宽" : "高") : (sx <= sy ? "宽" : "高");

    if (Math.abs(scale - 1) < 1e-9) {
        pencil.progressEnd();
        log("内容已经是目标大小（按" + edge + "匹配画布），无需缩放。");
        return;
    }

    log("画布 " + rect.width + "×" + rect.height
        + "，内容并集边框 " + uw + "×" + uh
        + "，缩放比例 " + scale.toFixed(4) + "（按画布" + edge + "匹配，模式 " + MODE + "）");

    var anchorX = ux + uw / 2;          // 内容并集中心（画布全局坐标）
    var anchorY = uy + uh / 2;
    var dstX = rect.x + rect.width / 2;  // 缩放后并集中心落到画布中心
    var dstY = rect.y + rect.height / 2;

    pencil.beginUndoGroup("脚本：批量缩放关键帧（" + MODE + "）");
    var done = 0;
    for (var j = 0; j < positions.length; j++) {
        if (!pencil.progressSetValue(positions.length + j)) { canceled = true; break; }
        if (pencil.scaleKeyFrame(idx, positions[j], scale, anchorX, anchorY, dstX, dstY)) done++;
    }
    pencil.endUndoGroup();
    pencil.progressEnd();

    log("已缩放 " + done + "/" + positions.length + " 个关键帧"
        + (canceled ? "（已取消，已处理的帧可 Ctrl+Z 撤销）" : "（Ctrl+Z 可一次撤销）"));
});
