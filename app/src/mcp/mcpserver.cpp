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

#include "mcpserver.h"

#include <QBuffer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>
#include <QUuid>

#include "editor.h"
#include "layermanager.h"
#include "mcpdispatcher.h"
#include "preferencemanager.h"
#include "util/pencildef.h"

McpServer* McpServer::sInstance = nullptr;

McpServer::McpServer(Editor* editor, QObject* parent) :
    QObject(parent),
    mEditor(editor)
{
    Q_ASSERT(sInstance == nullptr);
    sInstance = this;
    mDispatcher = new McpDispatcher(editor, this);
}

McpServer::~McpServer()
{
    stop();
    if (sInstance == this)
        sInstance = nullptr;
}

bool McpServer::start(quint16 port)
{
    if (isRunning() && mPort == port)
        return true;

    stop();

    mServer = new QTcpServer(this);
    connect(mServer, &QTcpServer::newConnection, this, &McpServer::onNewConnection);

    if (!mServer->listen(QHostAddress::LocalHost, port))
    {
        mLastError = tr("无法监听端口 %1：%2").arg(port).arg(mServer->errorString());
        mServer->deleteLater();
        mServer = nullptr;
        emit stateChanged();
        return false;
    }

    mPort = port;
    mLastError.clear();
    emit stateChanged();
    return true;
}

void McpServer::stop()
{
    if (mServer == nullptr)
        return;

    for (auto it = mRequests.begin(); it != mRequests.end(); ++it)
    {
        disconnect(it.key(), &QTcpSocket::readyRead, this, nullptr);
        disconnect(it.key(), &QTcpSocket::disconnected, this, nullptr);
        it.key()->disconnectFromHost();
    }
    mRequests.clear();

    mServer->close();
    mServer->deleteLater();
    mServer = nullptr;
    mPort = 0;
    emit stateChanged();
}

bool McpServer::isRunning() const
{
    return mServer != nullptr && mServer->isListening();
}

QString McpServer::token() const
{
    if (mEditor.isNull() || mEditor->preference() == nullptr)
        return QString();

    QString t = mEditor->preference()->getString(SETTING::MCP_TOKEN);
    if (t.isEmpty())
    {
        t = QUuid::createUuid().toString(QUuid::WithoutBraces);
        mEditor->preference()->set(SETTING::MCP_TOKEN, t);
    }
    return t;
}

void McpServer::refreshToken()
{
    if (mEditor.isNull() || mEditor->preference() == nullptr)
        return;

    const QString t = QUuid::createUuid().toString(QUuid::WithoutBraces);
    mEditor->preference()->set(SETTING::MCP_TOKEN, t);
}

void McpServer::onNewConnection()
{
    while (mServer->hasPendingConnections())
    {
        QTcpSocket* socket = mServer->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, &McpServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &McpServer::onDisconnected);
        mRequests.insert(socket, HttpReq());
    }
}

McpServer::HttpReq* McpServer::requestFor(QTcpSocket* socket)
{
    auto it = mRequests.find(socket);
    if (it == mRequests.end())
        return nullptr;
    return &it.value();
}

void McpServer::onReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket == nullptr)
        return;

    HttpReq* req = requestFor(socket);
    if (req == nullptr || req->responded)
        return;

    req->buffer.append(socket->readAll());

    if (!req->headerDone && !tryParseHeaders(req))
        return;

    if (req->headerDone)
    {
        const qint64 bodyLen = req->buffer.size() - req->bodyStart;
        if (req->contentLength >= 0 && bodyLen >= req->contentLength)
            handleRequest(socket, *req);
    }
}

void McpServer::onDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket == nullptr)
        return;
    mRequests.remove(socket);
    socket->deleteLater();
}

bool McpServer::tryParseHeaders(HttpReq* req)
{
    const int headerEnd = req->buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0)
    {
        if (req->buffer.size() > kMaxHeaderBytes)
        {
            // 没有完整头部就已超限
            req->responded = true;
            // socket 由 sender() 传递不便，这里复用 buffer 前缀保存发起 socket 不现实，
            // 超限直接由调用侧断开；此分支仅在 buffer 异常增长时触发
        }
        return false;
    }

    const QList<QByteArray> lines = req->buffer.left(headerEnd).split('\n');
    if (lines.isEmpty())
        return false;

    const QList<QByteArray> requestLine = lines[0].trimmed().split(' ');
    if (requestLine.size() < 2)
        return false;

    req->method = QString::fromLatin1(requestLine[0]).toUpper();
    req->path = QString::fromLatin1(requestLine[1]);
    req->headerDone = true;
    req->bodyStart = headerEnd + 4;

    for (int i = 1; i < lines.size(); ++i)
    {
        const int colon = lines[i].indexOf(':');
        if (colon < 0)
            continue;
        const QString name = QString::fromLatin1(lines[i].left(colon).trimmed()).toLower();
        const QString value = QString::fromLatin1(lines[i].mid(colon + 1).trimmed());
        req->headers.insert(name, value);
    }

    const auto contentLengthIt = req->headers.constFind("content-length");
    if (contentLengthIt != req->headers.constEnd())
        req->contentLength = contentLengthIt.value().toLongLong();

    return true;
}

bool McpServer::checkAuthorized(const HttpReq& req, QString* reason) const
{
    // 任何带 Origin 的请求一律拒绝：MCP 客户端不带 Origin，浏览器页面必带
    if (req.headers.contains("origin"))
    {
        *reason = QStringLiteral("请求带有 Origin 头，已被拒绝（禁止浏览器跨源访问）");
        return false;
    }

    // DNS rebinding 防护：Host 必须是本机回环
    const QString host = req.headers.value("host");
    if (!host.startsWith("127.0.0.1") && !host.startsWith("localhost") && !host.startsWith("[::1]"))
    {
        *reason = QStringLiteral("非法 Host");
        return false;
    }

    const QString expected = token();
    if (expected.isEmpty())
    {
        *reason = QStringLiteral("服务器尚未生成访问令牌");
        return false;
    }

    const auto authIt = req.headers.constFind("authorization");
    if (authIt != req.headers.constEnd())
    {
        const QString v = authIt.value().trimmed();
        if (v.startsWith("Bearer ", Qt::CaseInsensitive) && v.mid(7).trimmed() == expected)
            return true;
    }

    const auto pencilIt = req.headers.constFind("x-pencil-token");
    if (pencilIt != req.headers.constEnd() && pencilIt.value().trimmed() == expected)
        return true;

    const int query = req.path.indexOf('?');
    if (query >= 0)
    {
        const QStringList params = req.path.mid(query + 1).split('&');
        for (const QString& param : params)
        {
            if (param.startsWith("token=") && QUrl::fromPercentEncoding(param.mid(6).toUtf8()) == expected)
                return true;
        }
    }

    *reason = QStringLiteral("访问令牌缺失或不正确（支持 Authorization: Bearer、X-Pencil-Token 头或 ?token= 参数）");
    return false;
}

void McpServer::handleRequest(QTcpSocket* socket, const HttpReq& req)
{
    HttpReq* stored = requestFor(socket);
    if (stored == nullptr || stored->responded)
        return;
    stored->responded = true;

    if (req.method == "OPTIONS")
    {
        sendHttp(socket, 204, QByteArray(), "text/plain");
        return;
    }

    const QString pathOnly = req.path.left(req.path.indexOf('?'));

    // 免鉴权探活端点
    if (req.method == "GET" && (pathOnly == "/" || pathOnly == "/api/status"))
    {
        QJsonObject status;
        status.insert("name", QStringLiteral("pencil-dream"));
        status.insert("version", QStringLiteral(APP_VERSION));
        status.insert("mcp", true);
        sendJson(socket, status);
        return;
    }

    QString reason;
    if (!checkAuthorized(req, &reason))
    {
        sendHttp(socket, 401, reason.toUtf8(), "text/plain; charset=utf-8");
        return;
    }

    if (req.method == "POST" && (pathOnly == "/mcp" || pathOnly == "/jsonrpc" || pathOnly == "/"))
    {
        const QByteArray body = req.buffer.mid(req.bodyStart, static_cast<int>(req.contentLength > 0 ? req.contentLength : 0));
        if (body.size() > kMaxBodyBytes)
        {
            sendHttp(socket, 413, QByteArray("payload too large"), "text/plain");
            return;
        }

        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        {
            sendJsonRpcError(socket, QJsonValue(QJsonValue::Null), -32700,
                             tr("JSON 解析失败: %1").arg(parseError.errorString()));
            return;
        }
        processJsonRpc(socket, doc.object());
        return;
    }

    sendHttp(socket, 404, QByteArray("not found"), "text/plain");
}

void McpServer::processJsonRpc(QTcpSocket* socket, const QJsonObject& rpc)
{
    const QJsonValue id = rpc.value("id");
    const QString method = rpc.value("method").toString();

    // notification（无 id）：处理后不回包
    const bool wantsReply = id != QJsonValue::Undefined && !id.isNull();

    if (method.isEmpty() || rpc.value("jsonrpc").toString() != QStringLiteral("2.0"))
    {
        if (wantsReply)
            sendJsonRpcError(socket, id, -32600, tr("非法的 JSON-RPC 请求"));
        else
            sendHttp(socket, 204, QByteArray(), "text/plain");
        return;
    }

    // MCP 通知（如 notifications/initialized）
    if (method.startsWith("notifications/"))
    {
        sendHttp(socket, 202, QByteArray(), "text/plain");
        return;
    }

    if (method == "initialize")
    {
        const QString requested = rpc.value("params").toObject().value("protocolVersion").toString();
        QJsonObject serverInfo;
        serverInfo.insert("name", QStringLiteral("pencil-dream"));
        serverInfo.insert("version", QStringLiteral(APP_VERSION));

        QJsonObject capabilities;
        capabilities.insert("tools", QJsonObject());

        QJsonObject result;
        result.insert("protocolVersion", requested.isEmpty() ? QStringLiteral("2024-11-05") : requested);
        result.insert("capabilities", capabilities);
        result.insert("serverInfo", serverInfo);
        sendJsonRpcResult(socket, id, result);
        return;
    }

    if (method == "ping")
    {
        sendJsonRpcResult(socket, id, QJsonObject());
        return;
    }

    if (method == "tools/list")
    {
        QJsonObject result;
        result.insert("tools", mDispatcher->toolsSchema());
        sendJsonRpcResult(socket, id, result);
        return;
    }

    if (method == "tools/call")
    {
        const QJsonObject params = rpc.value("params").toObject();
        const QString name = params.value("name").toString();
        const QJsonObject args = params.value("arguments").toObject();

        if (mDispatching)
        {
            // 工具执行期间的嵌套/并发调用直接拒绝，避免主线程重入
            QJsonObject result;
            QJsonArray content;
            QJsonObject text;
            text.insert("type", "text");
            text.insert("text", tr("另一个工具调用正在进行中，请稍后重试"));
            content.append(text);
            result.insert("content", content);
            result.insert("isError", true);
            sendJsonRpcResult(socket, id, result);
            return;
        }

        mDispatching = true;
        McpDispatcher::ToolResult toolResult = mDispatcher->dispatch(name, args);
        mDispatching = false;

        QJsonArray content;
        if (!toolResult.image.isNull())
        {
            QByteArray png;
            QBuffer buffer(&png);
            buffer.open(QIODevice::WriteOnly);
            toolResult.image.save(&buffer, "PNG");
            QJsonObject imageContent;
            imageContent.insert("type", "image");
            imageContent.insert("data", QString::fromLatin1(png.toBase64()));
            imageContent.insert("mimeType", "image/png");
            content.append(imageContent);
        }
        QJsonObject text;
        text.insert("type", "text");
        text.insert("text", QString::fromUtf8(QJsonDocument(toolResult.data).toJson(QJsonDocument::Indented)));
        content.append(text);

        QJsonObject result;
        result.insert("content", content);
        if (!toolResult.data.isEmpty())
            result.insert("structuredContent", toolResult.data);
        if (toolResult.isError)
            result.insert("isError", true);
        sendJsonRpcResult(socket, id, result);
        return;
    }

    if (method == "resources/list" || method == "prompts/list")
    {
        QJsonObject result;
        result.insert(method == "resources/list" ? "resources" : "prompts", QJsonArray());
        sendJsonRpcResult(socket, id, result);
        return;
    }

    if (wantsReply)
        sendJsonRpcError(socket, id, -32601, tr("未知方法: %1").arg(method));
    else
        sendHttp(socket, 204, QByteArray(), "text/plain");
}

void McpServer::sendHttp(QTcpSocket* socket, int status, const QByteArray& body, const char* contentType)
{
    const char* statusText = "OK";
    switch (status)
    {
    case 200: statusText = "OK"; break;
    case 202: statusText = "Accepted"; break;
    case 204: statusText = "No Content"; break;
    case 401: statusText = "Unauthorized"; break;
    case 403: statusText = "Forbidden"; break;
    case 404: statusText = "Not Found"; break;
    case 413: statusText = "Payload Too Large"; break;
    default: break;
    }

    QByteArray header;
    header.append("HTTP/1.1 ");
    header.append(QByteArray::number(status));
    header.append(' ');
    header.append(statusText);
    header.append("\r\nContent-Type: ");
    header.append(contentType);
    header.append("\r\nContent-Length: ");
    header.append(QByteArray::number(body.size()));
    header.append("\r\nConnection: close\r\n\r\n");

    socket->write(header);
    if (!body.isEmpty())
        socket->write(body);
    socket->disconnectFromHost();
}

void McpServer::sendJson(QTcpSocket* socket, const QJsonObject& obj)
{
    sendHttp(socket, 200, QJsonDocument(obj).toJson(QJsonDocument::Compact), "application/json");
}

void McpServer::sendJsonRpcResult(QTcpSocket* socket, const QJsonValue& id, const QJsonObject& result)
{
    QJsonObject rpc;
    rpc.insert("jsonrpc", "2.0");
    rpc.insert("id", id);
    rpc.insert("result", result);
    sendJson(socket, rpc);
}

void McpServer::sendJsonRpcError(QTcpSocket* socket, const QJsonValue& id, int code, const QString& message)
{
    QJsonObject err;
    err.insert("code", code);
    err.insert("message", message);

    QJsonObject rpc;
    rpc.insert("jsonrpc", "2.0");
    rpc.insert("id", id);
    rpc.insert("error", err);
    sendJson(socket, rpc);
}
