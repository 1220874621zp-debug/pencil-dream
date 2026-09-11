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

#ifndef MCPPAGE_H
#define MCPPAGE_H

#include <QWidget>

class PreferenceManager;

namespace Ui
{
class McpPage;
}

/** 首选项「MCP 智能体」页：服务器启停/端口/令牌与客户端配置一键复制 */
class McpPage : public QWidget
{
    Q_OBJECT

public:
    explicit McpPage();
    ~McpPage() override;

    void setManager(PreferenceManager* p);
    void updateValues();

private slots:
    void enableChanged(int state);
    void autostartChanged(int state);
    void portChanged(int port);
    void regenToken();
    void copyToken();
    void restartServer();
    void copyClientConfig();
    void copyAgentPrompt();
    void refreshStatus();

private:
    QString currentToken() const;
    int currentPort() const;

    Ui::McpPage* ui = nullptr;
    PreferenceManager* mManager = nullptr;
};

#endif // MCPPAGE_H
