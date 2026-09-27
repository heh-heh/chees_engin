#include "RelayServer.h"

#include <QDateTime>
#include <QHostAddress>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtDebug>

RelayServer::RelayServer(quint16 port, QObject* parent)
    : QObject(parent), server_(new QTcpServer(this)), status_server_(new QTcpServer(this)) {
    connect(server_, &QTcpServer::newConnection, this, &RelayServer::onNewConnection);

    if (!server_->listen(QHostAddress::Any, port)) {
        qCritical().noquote() << "Failed to listen on port" << port << ":" << server_->errorString();
        return;
    }
    qInfo().noquote() << "Relay server listening on port" << port;

    connect(status_server_, &QTcpServer::newConnection, this, &RelayServer::onStatusConnection);
    if (!status_server_->listen(QHostAddress::Any, port + 1)) {
        qWarning().noquote() << "Status page unavailable on port" << port + 1 << ":" << status_server_->errorString();
    } else {
        qInfo().noquote() << "Relay status page listening on port" << port + 1;
    }
}

void RelayServer::onStatusConnection() {
    while (status_server_->hasPendingConnections()) {
        QTcpSocket* socket = status_server_->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            socket->readAll();
            const QByteArray body = buildStatusPage();
            const QByteArray response = "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                "Connection: close\r\n\r\n" + body;
            socket->write(response);
            socket->disconnectFromHost();
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}

QByteArray RelayServer::buildStatusPage() const {
    QSet<QString> paired_rooms;
    for (auto it = connections_.cbegin(); it != connections_.cend(); ++it) {
        if (it->paired && !it->roomCode.isEmpty()) {
            paired_rooms.insert(it->roomCode);
        }
    }

    QString rows;
    for (auto it = waitingHosts_.cbegin(); it != waitingHosts_.cend(); ++it) {
        rows += QString("<tr><td>%1</td><td>호스트 대기 중</td><td>1명</td></tr>")
            .arg(it.key().toHtmlEscaped());
    }
    for (const QString& room : paired_rooms) {
        rows += QString("<tr><td>%1</td><td>대국 중</td><td>2명</td></tr>")
            .arg(room.toHtmlEscaped());
    }
    if (rows.isEmpty()) {
        rows = "<tr><td colspan=\"3\">현재 생성된 방이 없습니다.</td></tr>";
    }

    const QString html = QString(
        "<!doctype html><html lang=\"ko\"><head>"
        "<meta charset=\"utf-8\"><meta http-equiv=\"refresh\" content=\"5\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<title>Chess Relay Status</title><style>"
        "body{font-family:system-ui,sans-serif;background:#10151b;color:#e8edf2;max-width:860px;margin:40px auto;padding:0 20px}"
        "h1{font-size:28px}.summary{display:flex;gap:12px;flex-wrap:wrap;margin:24px 0}"
        ".card{background:#1b242d;border:1px solid #34424f;border-radius:8px;padding:16px 20px;min-width:150px}"
        ".value{font-size:28px;font-weight:700;color:#80e6b3}.label{color:#9aa5b1;margin-top:4px}"
        "table{border-collapse:collapse;width:100%;background:#1b242d}th,td{text-align:left;padding:12px;border-bottom:1px solid #34424f}"
        "th{color:#9aa5b1;font-weight:500}footer{color:#77838f;margin-top:20px;font-size:13px}"
        "</style></head><body><h1>Chess Relay Server</h1><div class=\"summary\">"
        "<div class=\"card\"><div class=\"value\">%1</div><div class=\"label\">TCP 접속자</div></div>"
        "<div class=\"card\"><div class=\"value\">%2</div><div class=\"label\">대국 중인 방</div></div>"
        "<div class=\"card\"><div class=\"value\">%3</div><div class=\"label\">대기 중인 방</div></div>"
        "</div><table><thead><tr><th>방 코드</th><th>상태</th><th>접속자</th></tr></thead>"
        "<tbody>%4</tbody></table><footer>5초마다 자동 갱신 · 마지막 갱신: %5</footer>"
        "</body></html>")
        .arg(connections_.size())
        .arg(paired_rooms.size())
        .arg(waitingHosts_.size())
        .arg(rows)
        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    return html.toUtf8();
}

void RelayServer::onNewConnection() {
    while (server_->hasPendingConnections()) {
        QTcpSocket* socket = server_->nextPendingConnection();
        connections_.insert(socket, Connection{});
        connect(socket, &QTcpSocket::readyRead, this, &RelayServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &RelayServer::onSocketDisconnected);
    }
}

void RelayServer::onReadyRead() {
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket == nullptr) {
        return;
    }

    const auto it = connections_.find(socket);
    if (it == connections_.end()) {
        return;
    }

    if (it->paired) {
        QTcpSocket* peer = peers_.value(socket);
        if (peer != nullptr) {
            peer->write(socket->readAll());
        }
        return;
    }

    it->buffer.append(socket->readAll());
    const int newline_index = it->buffer.indexOf('\n');
    if (newline_index == -1) {
        return;
    }

    const QByteArray line = it->buffer.left(newline_index);
    it->buffer.remove(0, newline_index + 1);
    handleHandshakeLine(socket, line);
}

void RelayServer::handleHandshakeLine(QTcpSocket* socket, const QByteArray& line) {
    const QString text = QString::fromUtf8(line).trimmed();
    const QStringList parts = text.split('|');
    if (parts.size() != 3 || parts[0] != "HELLO" || (parts[1] != "HOST" && parts[1] != "CLIENT")) {
        socket->write("ERROR|invalid handshake\n");
        socket->disconnectFromHost();
        return;
    }

    const QString& role = parts[1];
    const QString& room_code = parts[2];
    if (room_code.isEmpty()) {
        socket->write("ERROR|room code required\n");
        socket->disconnectFromHost();
        return;
    }

    if (role == "HOST") {
        if (waitingHosts_.contains(room_code)) {
            socket->write("ERROR|room code already in use\n");
            socket->disconnectFromHost();
            return;
        }
        connections_[socket].roomCode = room_code;
        waitingHosts_.insert(room_code, socket);
        qInfo().noquote() << "Room" << room_code << "created, waiting for a client";
        return;
    }

    // role == "CLIENT"
    if (!waitingHosts_.contains(room_code)) {
        socket->write("ERROR|room not found\n");
        socket->disconnectFromHost();
        return;
    }

    QTcpSocket* host_socket = waitingHosts_.take(room_code);
    pairSockets(host_socket, socket);
    qInfo().noquote() << "Room" << room_code << "paired";
}

void RelayServer::pairSockets(QTcpSocket* hostSocket, QTcpSocket* clientSocket) {
    connections_[hostSocket].paired = true;
    connections_[clientSocket].paired = true;
    peers_.insert(hostSocket, clientSocket);
    peers_.insert(clientSocket, hostSocket);

    hostSocket->write("PAIRED\n");
    clientSocket->write("PAIRED\n");

    // Forward any game bytes that arrived immediately after the handshake line.
    QByteArray& host_leftover = connections_[hostSocket].buffer;
    if (!host_leftover.isEmpty()) {
        clientSocket->write(host_leftover);
        host_leftover.clear();
    }
    QByteArray& client_leftover = connections_[clientSocket].buffer;
    if (!client_leftover.isEmpty()) {
        hostSocket->write(client_leftover);
        client_leftover.clear();
    }
}

void RelayServer::onSocketDisconnected() {
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket == nullptr) {
        return;
    }
    teardown(socket);
}

void RelayServer::teardown(QTcpSocket* socket) {
    const auto it = connections_.find(socket);
    if (it != connections_.end()) {
        if (!it->paired && !it->roomCode.isEmpty()) {
            waitingHosts_.remove(it->roomCode);
        }
        connections_.erase(it);
    }

    QTcpSocket* peer = peers_.take(socket);
    if (peer != nullptr) {
        peers_.remove(peer);
        peer->disconnectFromHost();
    }

    socket->deleteLater();
}
