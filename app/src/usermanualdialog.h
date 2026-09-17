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
#ifndef USERMANUALDIALOG_H
#define USERMANUALDIALOG_H

#include <QDialog>
#include <QMap>
#include <QString>

class QLineEdit;
class QTextBrowser;
class QTreeWidget;
class QTreeWidgetItem;

class UserManualDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UserManualDialog(QWidget* parent = nullptr);

private:
    struct ManualDoc
    {
        QString resourcePath;   // 形如 :/manual/import-export/xxx.md
        QString title;          // 第一个 # 标题，兜底显示名
        QString body;           // 原文，供搜索
    };

    void buildUi();
    void scanResources();
    void buildTree();
    void showPage(const QString& resourcePath);
    void applyFilter(const QString& text);

    QLineEdit*    mSearchBox = nullptr;
    QTreeWidget*  mTree = nullptr;
    QTextBrowser* mContent = nullptr;

    QMap<QString, ManualDoc> mDocs;  // key 为 resourcePath
    QString mCurrentPage;
};

#endif // USERMANUALDIALOG_H
