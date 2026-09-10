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

#include "projectmanagerdialog.h"

#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QThreadPool>
#include <QToolButton>
#include <QVBoxLayout>

#include "app_util.h"
#include "filedialog.h"
#include "pencildef.h"
#include "projectthumb.h"
#include "theme.h"

namespace
{
    // 卡片视觉：固定宽 224，缩略图 208x117（16:9），2x DPR
    constexpr int CARD_W = 224;
    constexpr int THUMB_W = 208;
    constexpr int THUMB_H = 117;
    constexpr int GRID_COLUMNS = 4;

    QStringList recentProjects()
    {
        QSettings settings(PENCIL2D, PENCIL2D);
        return settings.value(QStringLiteral("RecentFiles")).toStringList();
    }

    void setRecentProjects(const QStringList& projects)
    {
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.setValue(QStringLiteral("RecentFiles"), QVariant(projects));
    }

    QPixmap placeholderThumb()
    {
        QPixmap pm(THUMB_W * 2, THUMB_H * 2);
        pm.setDevicePixelRatio(2.0);
        pm.fill(QColor(0x2A, 0x2A, 0x31));
        return pm;
    }
}

ProjectManagerDialog::ProjectManagerDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("工程管理"));
    setMinimumSize(880, 540);
    resize(1040, 620);
    hideQuestionMark(*this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(12);

    // 顶部：标题 + 操作按钮
    auto* titleRow = new QHBoxLayout;
    titleRow->setSpacing(8);
    auto* title = new QLabel(tr("工程管理"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(13);
    titleFont.setBold(true);
    title->setFont(titleFont);
    auto* browseButton = new QPushButton(tr("打开其他工程…"), this);
    auto* newButton = new QPushButton(tr("新建工程"), this);
    newButton->setObjectName(QStringLiteral("projectNewButton"));
    titleRow->addWidget(title);
    titleRow->addStretch(1);
    titleRow->addWidget(browseButton);
    titleRow->addWidget(newButton);
    root->addLayout(titleRow);

    // 卡片滚动区
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    mGridHost = new QWidget(scroll);
    mGrid = new QGridLayout(mGridHost);
    mGrid->setContentsMargins(4, 4, 4, 4);
    mGrid->setHorizontalSpacing(16);
    mGrid->setVerticalSpacing(16);
    mGrid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    scroll->setWidget(mGridHost);
    root->addWidget(scroll, 1);

    mEmptyHint = new QLabel(tr("最近没有工程，点击右上角「新建工程」开始创作"), mGridHost);
    mEmptyHint->setStyleSheet(QStringLiteral("color: #8A8A90;"));
    mGrid->addWidget(mEmptyHint, 0, 0);

    setStyleSheet(QStringLiteral(
        "QFrame#projectCard { background: #232329; border: 1px solid #2E2E36; border-radius: 8px; }"
        "QFrame#projectCard:hover { border: 1px solid %1; }"
        "QPushButton#projectNewButton { background: %1; color: white; border: none; border-radius: 4px; padding: 5px 14px; }"
        "QPushButton#projectNewButton:hover { background: %2; }"
    ).arg(Theme::Accent.name(), Theme::AccentHover.name()));

    connect(newButton, &QPushButton::clicked, this, [this]
    {
        done(NewProject);
    });

    connect(browseButton, &QPushButton::clicked, this, [this]
    {
        const QString path = FileDialog::getOpenFileName(this, FileType::ANIMATION, tr("打开工程"));
        if (!path.isEmpty())
        {
            openProject(path);
        }
    });

    buildCards();
}

bool ProjectManagerDialog::eventFilter(QObject* watched, QEvent* event)
{
    // 整卡点击=打开（卡片自身收到按压；标签子控件不消费按压会自动冒泡到卡）
    if (event->type() == QEvent::MouseButtonPress)
    {
        const auto* mouseEvent = static_cast<const QMouseEvent*>(event);
        const QString path = mCards.key(static_cast<QFrame*>(watched));
        if (mouseEvent->button() == Qt::LeftButton && !path.isEmpty())
        {
            openProject(path);
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void ProjectManagerDialog::buildCards()
{
    const QStringList projects = recentProjects();
    for (const QString& path : projects)
    {
        if (!QFile::exists(path)) { continue; }

        auto* card = new QFrame(mGridHost);
        card->setObjectName(QStringLiteral("projectCard"));
        card->setFixedWidth(CARD_W);
        card->setCursor(Qt::PointingHandCursor);
        card->installEventFilter(this);

        auto* cardLay = new QVBoxLayout(card);
        cardLay->setContentsMargins(8, 8, 8, 8);
        cardLay->setSpacing(6);

        auto* thumb = new QLabel(card);
        thumb->setObjectName(QStringLiteral("projectThumb"));
        thumb->setFixedSize(THUMB_W, THUMB_H);
        thumb->setAlignment(Qt::AlignCenter);
        thumb->setPixmap(placeholderThumb());
        cardLay->addWidget(thumb, 0, Qt::AlignHCenter);

        const QFileInfo info(path);
        auto* nameRow = new QHBoxLayout;
        nameRow->setSpacing(4);
        auto* nameLabel = new QLabel(info.completeBaseName(), card);
        nameLabel->setToolTip(QDir::toNativeSeparators(info.absoluteFilePath()));
        auto* deleteButton = new QToolButton(card);
        deleteButton->setIcon(QIcon(QStringLiteral(":/icons/themes/playful/timeline/layer-remove.svg")));
        deleteButton->setToolTip(tr("删除工程（移入回收站）"));
        nameRow->addWidget(nameLabel, 1);
        nameRow->addWidget(deleteButton, 0);
        cardLay->addLayout(nameRow);

        auto* dateLabel = new QLabel(info.lastModified().date().toString(QStringLiteral("yyyy-MM-dd")), card);
        dateLabel->setStyleSheet(QStringLiteral("color: #8A8A90; font-size: 9px;"));
        cardLay->addWidget(dateLabel);

        const int index = mCards.size();
        mGrid->addWidget(card, 1 + index / GRID_COLUMNS, index % GRID_COLUMNS);
        mCards.insert(path, card);

        connect(deleteButton, &QToolButton::clicked, this, [this, path]
        {
            const QMessageBox::StandardButton choice = QMessageBox::warning(
                this, tr("删除工程"),
                tr("将把工程文件移入回收站：\n%1").arg(QDir::toNativeSeparators(path)),
                QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
            if (choice != QMessageBox::Ok) { return; }

            if (!QFile::moveToTrash(path))
            {
                QMessageBox::information(this, tr("删除工程"), tr("删除失败，文件可能被占用。"));
                return;
            }
            QStringList recents = recentProjects();
            recents.removeAll(path);
            setRecentProjects(recents);
            removeCard(path);
        });

        requestThumbnail(path);
    }
    mEmptyHint->setVisible(mCards.isEmpty());
}

void ProjectManagerDialog::removeCard(const QString& pclxPath)
{
    QFrame* card = mCards.take(pclxPath);
    if (card == nullptr) { return; }
    card->deleteLater();

    // 按最近列表顺序重排剩余卡片
    const QStringList order = recentProjects();
    for (QFrame* c : mCards)
    {
        mGrid->removeWidget(c);
    }
    int index = 0;
    for (const QString& path : order)
    {
        QFrame* c = mCards.value(path);
        if (c == nullptr) { continue; }
        mGrid->addWidget(c, 1 + index / GRID_COLUMNS, index % GRID_COLUMNS);
        ++index;
    }
    mEmptyHint->setVisible(mCards.isEmpty());
}

void ProjectManagerDialog::openProject(const QString& pclxPath)
{
    mSelectedProject = pclxPath;
    done(OpenProject);
}

void ProjectManagerDialog::requestThumbnail(const QString& pclxPath)
{
    QPointer<ProjectManagerDialog> guard(this);
    const QString path = pclxPath;
    QThreadPool::globalInstance()->start([guard, path]
    {
        const QImage thumb = ProjectThumb::load(path);
        if (guard != nullptr)
        {
            QMetaObject::invokeMethod(guard, "onThumbReady",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, path),
                                      Q_ARG(QImage, thumb));
        }
    });
}

void ProjectManagerDialog::onThumbReady(const QString& pclxPath, const QImage& thumb)
{
    QFrame* card = mCards.value(pclxPath);
    if (card == nullptr) { return; }

    auto* thumbLabel = card->findChild<QLabel*>(QStringLiteral("projectThumb"));
    if (thumbLabel == nullptr) { return; }

    QPixmap pm;
    if (!thumb.isNull())
    {
        pm = QPixmap::fromImage(thumb.scaled(THUMB_W * 2, THUMB_H * 2, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        pm.setDevicePixelRatio(2.0);
    }
    if (!pm.isNull())
    {
        thumbLabel->setPixmap(pm);
    }
}
