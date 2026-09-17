// Pencil Dream 脚本：按实际图像像素裁剪关键帧
//
// 场景：图片（如扫描稿/照片）经「滤镜 → 颜色转为透明度」变成线稿后，
// 四周留下大片透明边距，图像尺寸仍是原图大小。本脚本把图层上每个关键帧
// 按画布上的实际内容像素（非透明部分）计算边框，裁剪成该边框的宽高。
//
// 特性：
//   1. 每帧各自裁剪到自己的紧致内容边框（去掉透明边距，尺寸最小化）；
//   2. 内容像素在画布上的位置不变（动画对位、画布显示不受影响）；
//   3. 空帧（无任何不透明像素）自动跳过；
//   4. 整个操作合并为一步撤销（Ctrl+Z 一次回滚）。
//
// 本文件位于脚本文件夹（菜单「脚本 → 打开脚本文件夹」），可直接修改后
// 用「脚本 → 重新载入脚本」生效，无需重启。

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
    log("图层：" + pencil.layerName(idx));

    var positions = pencil.keyFramePositions(idx);
    if (positions.length === 0) {
        alert("当前图层没有关键帧。");
        return;
    }

    pencil.beginUndoGroup("脚本：按实际像素裁剪关键帧");
    var done = 0, skipped = 0;
    var minWidth = 0, minHeight = 0, maxWidth = 0, maxHeight = 0;
    for (var i = 0; i < positions.length; i++) {
        var pos = positions[i];
        var b = pencil.keyFrameBounds(idx, pos);
        if (!b || !b.width || !b.height || b.width <= 0 || b.height <= 0) {
            skipped++;
            log("第 " + pos + " 帧：空帧，跳过");
            continue;
        }
        if (pencil.cropKeyFrame(idx, pos, b.x, b.y, b.width, b.height)) {
            done++;
            log("第 " + pos + " 帧：裁剪为 " + b.width + "×" + b.height + "@(" + b.x + "," + b.y + ")");
            if (b.width > maxWidth) maxWidth = b.width;
            if (b.height > maxHeight) maxHeight = b.height;
            if (minWidth === 0 || b.width < minWidth) minWidth = b.width;
            if (minHeight === 0 || b.height < minHeight) minHeight = b.height;
        }
    }
    pencil.endUndoGroup();

    if (done === 0) {
        alert("没有可裁剪的帧（所有关键帧都是空的）。");
        return;
    }
    log("共裁剪 " + done + " 帧，帧尺寸范围 "
        + minWidth + "~" + maxWidth + " × " + minHeight + "~" + maxHeight
        + (skipped > 0 ? "，跳过空帧 " + skipped + " 个" : ""));
    alert("完成：已按实际内容像素裁剪 " + done + " 个关键帧"
        + (skipped > 0 ? "（跳过空帧 " + skipped + " 个）" : "")
        + "，内容位置不变（Ctrl+Z 可一次撤销）。");
});
