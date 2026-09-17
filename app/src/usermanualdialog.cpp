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
#include "usermanualdialog.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSettings>
#include <QSplitter>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
    const QString kResRoot = ":/manual";
    const QString kIndexPath = ":/manual/INDEX.md";

    // 资源里有、但 INDEX.md 没收录的文档归入此分组
    const QString kUncategorized = QStringLiteral("未收录");

    const QRegularExpression kHeadingRx("^#{2,6}\\s+(.+)$");
    const QRegularExpression kLinkRx("\\[([^\\]]+)\\]\\(([^)\\s]+\\.md)\\)");
    const QRegularExpression kTitleRx("^#\\s+(.+)$");

    QString docTitleFromBody(const QString& body, const QString& fallbackName)
    {
        for (const QString& line : body.split('\n'))
        {
            const auto match = kTitleRx.match(line);
            if (match.hasMatch())
                return match.captured(1).trimmed();
        }
        return fallbackName;
    }
}

UserManualDialog::UserManualDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Pencil Dream 使用手册"));
    resize(980, 660);

    scanResources();
    buildUi();
    buildTree();

    const QString lastPage = QSettings().value("manual/lastPage").toString();
    showPage(mDocs.contains(lastPage) ? lastPage : kIndexPath);
}

void UserManualDialog::scanResources()
{
    QDirIterator it(kResRoot, { "*.md" }, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        const QString path = it.next();

        QFile file(path);
        if (!file.open(QFile::ReadOnly | QFile::Text))
            continue;

        ManualDoc doc;
        doc.resourcePath = path;
        doc.body = QString::fromUtf8(file.readAll());
        doc.title = docTitleFromBody(doc.body, QFileInfo(path).fileName());
        mDocs.insert(path, doc);
    }
}

void UserManualDialog::buildUi()
{
    mSearchBox = new QLineEdit(this);
    mSearchBox->setPlaceholderText(tr("搜索标题或正文…"));
    mSearchBox->setClearButtonEnabled(true);

    mTree = new QTreeWidget(this);
    mTree->setHeaderHidden(true);
    mTree->setUniformRowHeights(true);

    mContent = new QTextBrowser(this);
    mContent->setOpenExternalLinks(true);

    auto* treeBox = new QVBoxLayout;
    treeBox->setContentsMargins(0, 0, 0, 0);
    treeBox->addWidget(mSearchBox);
    treeBox->addWidget(mTree, 1);

    auto* treePanel = new QWidget(this);
    treePanel->setLayout(treeBox);

    auto* splitter = new QSplitter(this);
    splitter->addWidget(treePanel);
    splitter->addWidget(mContent);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({ 300, 680 });

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(splitter);

    connect(mTree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* current, QTreeWidgetItem* /*previous*/)
    {
        if (current != nullptr && !current->data(0, Qt::UserRole).isNull())
            showPage(current->data(0, Qt::UserRole).toString());
    });

    connect(mSearchBox, &QLineEdit::textChanged, this, &UserManualDialog::applyFilter);

    // 手册内文档之间的相对链接：按当前页所在目录解析后跳转
    connect(mContent, &QTextBrowser::anchorClicked, this, [this](const QUrl& url)
    {
        if (!url.isRelative())
            return; // 外链已由 openExternalLinks 处理
        const QString target = QDir::cleanPath(
                    QDir(QFileInfo(mCurrentPage).absolutePath()).filePath(url.toString()));
        if (mDocs.contains(target))
            showPage(target);
    });
}

void UserManualDialog::buildTree()
{
    mTree->clear();

    auto makeItem = [this](const QString& label, const QString& path, QTreeWidgetItem* parent)
    {
        auto* item = (parent != nullptr) ? new QTreeWidgetItem(parent) : new QTreeWidgetItem;
        item->setText(0, label);
        item->setData(0, Qt::UserRole, path);
        if (parent == nullptr)
            mTree->addTopLevelItem(item);
        return item;
    };

    // 首页（总索引）始终在第一位
    if (mDocs.contains(kIndexPath))
        makeItem(tr("总索引"), kIndexPath, nullptr);

    // 目录树结构完全由 INDEX.md 驱动：二级及以上标题生成分组，组内链接生成条目
    QStringList referenced;
    const auto indexIt = mDocs.constFind(kIndexPath);
    if (indexIt != mDocs.constEnd())
    {
        QTreeWidgetItem* section = nullptr;
        for (const QString& line : indexIt->body.split('\n'))
        {
            const auto heading = kHeadingRx.match(line);
            if (heading.hasMatch())
            {
                section = makeItem(heading.captured(1).trimmed(), QString(), nullptr);
                continue;
            }
            const auto link = kLinkRx.match(line);
            if (link.hasMatch() && section != nullptr)
            {
                const QString path = QDir::cleanPath(QDir(kResRoot).filePath(link.captured(2)));
                makeItem(link.captured(1), path, section);
                referenced.append(path);
            }
        }
    }

    // 兜底：登记进了 manual.qrc 但 INDEX.md 没收录的文档，避免静默不可见
    auto uncategorized = new QTreeWidgetItem;
    bool hasUncategorized = false;
    for (auto docIt = mDocs.constBegin(); docIt != mDocs.constEnd(); ++docIt)
    {
        if (docIt.key() == kIndexPath || referenced.contains(docIt.key()))
            continue;
        if (!hasUncategorized)
        {
            uncategorized->setText(0, kUncategorized);
            mTree->addTopLevelItem(uncategorized);
            hasUncategorized = true;
        }
        makeItem(docIt->title, docIt.key(), uncategorized);
    }

    mTree->expandAll();
}

void UserManualDialog::showPage(const QString& resourcePath)
{
    mCurrentPage = resourcePath;
    QSettings().setValue("manual/lastPage", resourcePath);

    const auto it = mDocs.constFind(resourcePath);
    if (it == mDocs.constEnd())
    {
        mContent->setMarkdown(
                    tr("## 文档缺失\n\n`%1` 没有登记进 `manual.qrc`，请参考 manual/README.md 的维护说明补登记。")
                    .arg(resourcePath));
        return;
    }
    mContent->setMarkdown(it->body);

    // 内容里的链接跳转后，同步选中左侧目录里的对应条目
    for (QTreeWidgetItemIterator treeIt(mTree); *treeIt; ++treeIt)
    {
        if ((*treeIt)->data(0, Qt::UserRole).toString() == resourcePath)
        {
            QSignalBlocker blocker(mTree);
            mTree->setCurrentItem(*treeIt);
            break;
        }
    }
}

void UserManualDialog::applyFilter(const QString& text)
{
    const QString needle = text.trimmed();
    for (QTreeWidgetItemIterator it(mTree); *it; ++it)
    {
        QTreeWidgetItem* item = *it;
        const QString path = item->data(0, Qt::UserRole).toString();
        if (path.isEmpty())
            continue; // 分组节点，稍后按子节点可见性处理
        const auto docIt = mDocs.constFind(path);
        const QString haystack = (docIt != mDocs.constEnd())
                ? docIt->title + "\n" + docIt->body
                : item->text(0);
        item->setHidden(!needle.isEmpty() && !haystack.contains(needle, Qt::CaseInsensitive));
    }
    for (int i = 0; i < mTree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* section = mTree->topLevelItem(i);
        bool anyVisible = false;
        for (int c = 0; c < section->childCount(); ++c)
        {
            if (!section->child(c)->isHidden())
            {
                anyVisible = true;
                break;
            }
        }
        section->setHidden(!anyVisible);
    }
}
