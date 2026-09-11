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

#ifndef MCPSERVER_H
#define MCPSERVER_H

#include <QObject>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>

class Editor;
class McpDispatcher;

/**
 * 内嵌 MCP 服务器：手写 HTTP/1.1 + JSON-RPC 2.0 + MCP 协议。
 *
 * - 只监听 127.0.0.1，主线程信号槽驱动（无锁；重活由 dispatcher 派发）
 * - 鉴权：token（Authorization: Bearer / X-Pencil-Token / ?token= 三通道）；
 *   带 Origin 头一律拒绝（防浏览器跨源）；Host 必须是本机回环（防 DNS rebinding）
 * - GET / 与 GET /api/status 免鉴权，供客户端探活
 * - 端点：POST /mcp、/jsonrpc、/ （JSON-RPC）；其余 404
 */
class McpServer : public QObject
{
    Q_OBJECT

public:
    static McpServer* instance() { return sInstance; }

    McpServer(Editor* editor, QObject* parent = nullptr);
    ~McpServer() override;

    bool start(quint16 port);
    void stop();
    bool isRunning() const;
    quint16 port() const { return mPort; }
    QString lastError() const { return mLastError; }

    /** 读取（或惰性生成）访问令牌 */
    QString token() const;
    /** 重新生成访问令牌并持久化 */
    void refreshToken();

signals:
    void stateChanged();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    struct HttpReq
    {
        QByteArray buffer;
        QString method;
        QString path;
        QMap<QString, QString> headers; // key 全部小写
        qint64 contentLength = -1;
        int bodyStart = -1;
        bool headerDone = false;
        bool responded = false;
    };

    HttpReq* requestFor(QTcpSocket* socket);
    bool tryParseHeaders(HttpReq* req);
    void handleRequest(QTcpSocket* socket, const HttpReq& req);
    bool checkAuthorized(const HttpReq& req, QString* reason) const;
    void processJsonRpc(QTcpSocket* socket, const QJsonObject& rpc);

    void sendHttp(QTcpSocket* socket, int status, const QByteArray& body, const char* contentType);
    void sendJson(QTcpSocket* socket, const QJsonObject& obj);
    void sendJsonRpcResult(QTcpSocket* socket, const QJsonValue& id, const QJsonObject& result);
    void sendJsonRpcError(QTcpSocket* socket, const QJsonValue& id, int code, const QString& message);

    QPointer<Editor> mEditor;
    McpDispatcher* mDispatcher = nullptr;

    QTcpServer* mServer = nullptr;
    QHash<QTcpSocket*, HttpReq> mRequests;
    quint16 mPort = 0;
    QString mLastError;
    bool mDispatching = false; // 一次只跑一个工具（慢工具等待期间拒绝新调用）

    static McpServer* sInstance;

    static constexpr int kMaxHeaderBytes = 64 * 1024;
    static constexpr int kMaxBodyBytes = 32 * 1024 * 1024;
};

#endif // MCPSERVER_H
