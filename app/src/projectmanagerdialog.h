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

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/

#ifndef PROJECTMANAGERDIALOG_H
#define PROJECTMANAGERDIALOG_H

#include <QDialog>
#include <QFrame>
#include <QMap>

class QGridLayout;
class QLabel;

// 启动时的工程管理面板：最近工程卡片（16:9 缩略图/名称/修改日期），
// 整卡点击=打开，卡上删除钮=移入回收站并移出最近列表；顶部支持新建/浏览打开。
class ProjectManagerDialog : public QDialog
{
    Q_OBJECT

public:
    enum ResultCode
    {
        OpenProject = 2, // 非标准 QDialog 返回值，区分"打开了某工程"
        NewProject = 3,
    };

    explicit ProjectManagerDialog(QWidget* parent = nullptr);

    // ResultCode::OpenProject 时有效
    QString selectedProject() const { return mSelectedProject; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onThumbReady(const QString& pclxPath, const QImage& thumb);

private:
    void buildCards();
    void removeCard(const QString& pclxPath);
    void openProject(const QString& pclxPath);
    void requestThumbnail(const QString& pclxPath);

    QString mSelectedProject;
    QGridLayout* mGrid = nullptr;
    QWidget* mGridHost = nullptr;
    QLabel* mEmptyHint = nullptr;
    QMap<QString, QFrame*> mCards;
};

#endif // PROJECTMANAGERDIALOG_H
