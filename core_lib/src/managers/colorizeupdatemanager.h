/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef COLORIZEUPDATEMANAGER_H
#define COLORIZEUPDATEMANAGER_H

#include "basemanager.h"

#include <QPair>
#include <QSet>

class LayerColorize;

/*
 * 智能填色后台计算管理器：
 * - 主线程快照输入（线稿图 + 笔画图 + 计算域），工作线程跑分水岭
 * - 完成后回主线程写入缓存并整清画布缓存重绘
 * - (图层id, 关键帧位置) 去重，同一任务在飞时不重复入队
 */
class ColorizeUpdateManager : public BaseManager
{
    Q_OBJECT

public:
    explicit ColorizeUpdateManager(Editor* editor);
    ~ColorizeUpdateManager() override;

    bool init() override;
    Status load(Object*) override;
    Status save(Object*) override;

    /** 渲染期懒触发：当前帧上所有可见填色层的过期帧入队 */
    void requestVisibleUpdates();

    /** 手动触发某层某帧（覆盖帧解析为关键帧） */
    void requestUpdate(LayerColorize* layer, int frameNumber);

signals:
    /** 着色结果回贴完成（layerId/keyPos），UI 据此刷新时间轴待更新标记 */
    void frameUpdated(int layerId, int keyPos);

private slots:
    void applyResult(int layerId, int keyPos, const QImage& result, const QRect& bounds);

private:
    void enqueueJob(LayerColorize* layer, int frameNumber);
    bool takeJobRecord(int layerId, int keyPos);

    QSet<QPair<int, int>> mInFlight;
    bool mShutdown = false;

    friend class ColorizeUpdateRunnable;
};

#endif // COLORIZEUPDATEMANAGER_H
