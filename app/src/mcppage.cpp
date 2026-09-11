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

#include "mcppage.h"

#include <QApplication>
#include <QClipboard>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include "mcp/mcpserver.h"
#include "preferencemanager.h"
#include "util/pencildef.h"

#include "ui_mcppage.h"

McpPage::McpPage() :
    ui(new Ui::McpPage)
{
    ui->setupUi(this);

    connect(ui->enableCheckBox, &QCheckBox::stateChanged, this, &McpPage::enableChanged);
    connect(ui->autostartCheckBox, &QCheckBox::stateChanged, this, &McpPage::autostartChanged);
    connect(ui->portSpinBox, &QSpinBox::valueChanged, this, &McpPage::portChanged);
    connect(ui->regenTokenBtn, &QPushButton::clicked, this, &McpPage::regenToken);
    connect(ui->copyTokenBtn, &QPushButton::clicked, this, &McpPage::copyToken);
    connect(ui->restartBtn, &QPushButton::clicked, this, &McpPage::restartServer);
    connect(ui->copyConfigBtn, &QPushButton::clicked, this, &McpPage::copyClientConfig);
    connect(ui->copyPromptBtn, &QPushButton::clicked, this, &McpPage::copyAgentPrompt);
}

McpPage::~McpPage()
{
    delete ui;
}

void McpPage::setManager(PreferenceManager* p)
{
    mManager = p;
    if (McpServer* server = McpServer::instance())
        connect(server, &McpServer::stateChanged, this, &McpPage::refreshStatus);
}

void McpPage::updateValues()
{
    if (mManager == nullptr)
        return;

    QSignalBlocker blockEnable(ui->enableCheckBox);
    QSignalBlocker blockAutostart(ui->autostartCheckBox);
    QSignalBlocker blockPort(ui->portSpinBox);

    ui->enableCheckBox->setChecked(mManager->isOn(SETTING::MCP_ENABLED));
    ui->autostartCheckBox->setChecked(mManager->isOn(SETTING::MCP_AUTOSTART));
    ui->portSpinBox->setValue(mManager->getInt(SETTING::MCP_PORT));
    ui->tokenEdit->setText(currentToken());

    refreshStatus();
}

void McpPage::enableChanged(int state)
{
    if (mManager)
        mManager->set(SETTING::MCP_ENABLED, state != Qt::Unchecked);
}

void McpPage::autostartChanged(int state)
{
    if (mManager)
        mManager->set(SETTING::MCP_AUTOSTART, state != Qt::Unchecked);
}

void McpPage::portChanged(int port)
{
    if (mManager)
        mManager->set(SETTING::MCP_PORT, port);
}

void McpPage::regenToken()
{
    if (mManager == nullptr)
        return;

    mManager->set(SETTING::MCP_TOKEN, QUuid::createUuid().toString(QUuid::WithoutBraces));
    ui->tokenEdit->setText(currentToken());
}

void McpPage::copyToken()
{
    QApplication::clipboard()->setText(currentToken());
}

void McpPage::restartServer()
{
    McpServer* server = McpServer::instance();
    if (server == nullptr)
        return;

    server->stop();
    if (mManager && mManager->isOn(SETTING::MCP_ENABLED))
        server->start(static_cast<quint16>(currentPort()));

    refreshStatus();
}

void McpPage::copyClientConfig()
{
    QJsonObject pencil;
    pencil.insert("type", "http");
    pencil.insert("url", QStringLiteral("http://127.0.0.1:%1/mcp").arg(currentPort()));
    QJsonObject headers;
    headers.insert("Authorization", QStringLiteral("Bearer %1").arg(currentToken()));
    pencil.insert("headers", headers);

    QJsonObject servers;
    servers.insert("pencil-dream", pencil);

    QJsonObject config;
    config.insert("mcpServers", servers);

    QApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Indented)));
}

void McpPage::copyAgentPrompt()
{
    QApplication::clipboard()->setText(tr(
        "你是 Pencil Dream（传统二维动画软件）的操作智能体，通过 MCP 工具直接操控软件完成动画工作。\n"
        "工作方式：\n"
        "1. 先调用 get_scene_status 了解图层与关键帧结构；用 get_frame_image 查看画面再做决策。\n"
        "2. 图层引用：数字=图层id（优先）或时间轴行号（顶行=1），字符串=图层名称；引用失败时错误信息会附带当前图层清单。\n"
        "3. 坐标系：画布左上角为(0,0)，x向右、y向下；get_frame_image 返回的图像与 draw_stroke 等绘制工具使用同一坐标系。\n"
        "4. 所有修改都可撤销（undo 工具等效 Ctrl+Z）；每次改动后用 get_frame_image 复查效果，不满意就调整再查。\n"
        "5. 自动填色：create_layer(type=colorize) 建填色层 → draw_stroke 在填色层对应帧点彩色种子（小笔画即可） → "
        "request_colorize_update 触发计算 → 看图复查、补种子或调 set_colorize_options。\n"
        "6. 自动画中割：用 get_frame_image 看两张原画 → generate_inbetweens 在两帧之间生成中间帧 → 逐帧查看并微调。\n"
        "7. 涉及打开/覆盖工程的危险操作，先向用户确认。"));
}

void McpPage::refreshStatus()
{
    McpServer* server = McpServer::instance();
    const bool running = server && server->isRunning();

    ui->statusValueLabel->setText(running ? tr("运行中") : tr("已停止"));
    ui->urlValueLabel->setText(running
        ? QStringLiteral("http://127.0.0.1:%1/mcp").arg(server->port())
        : QStringLiteral("—"));

    if (!running && server && !server->lastError().isEmpty())
        ui->statusValueLabel->setText(tr("已停止（%1）").arg(server->lastError()));
}

QString McpPage::currentToken() const
{
    if (mManager == nullptr)
        return QString();

    QString token = mManager->getString(SETTING::MCP_TOKEN);
    if (token.isEmpty())
    {
        // 惰性生成：第一次访问时落一个新令牌
        token = QUuid::createUuid().toString(QUuid::WithoutBraces);
        mManager->set(SETTING::MCP_TOKEN, token);
    }
    return token;
}

int McpPage::currentPort() const
{
    return mManager ? mManager->getInt(SETTING::MCP_PORT) : 9528;
}
