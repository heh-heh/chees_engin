#include "NetworkSession.h"

#include <QHostAddress>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>

NetworkSession::NetworkSession(QObject* parent) : QObject(parent) {}

NetworkSession::~NetworkSession() {
    stop();
}

bool NetworkSession::hostGame(quint16 port, QString& error) {
    stop();
    mode_ = Mode::Host;
    server_ = new QTcpServer(this);
    connect(server_, &QTcpServer::newConnection, this, &NetworkSession::handleNewConnection);

    if (!server_->listen(QHostAddress::Any, port)) {
        error = server_->errorString();
        server_->deleteLater();
        server_ = nullptr;
        mode_ = Mode::None;
        return false;
    }
    return true;
}

bool NetworkSession::joinGame(const QString& host, quint16 port, QString& error) {
    if (host.isEmpty()) {
        error = "Enter the host LAN address.";
        return false;
    }

    stop();
    mode_ = Mode::Client;
    socket_ = new QTcpSocket(this);
    attachSocket(socket_);
    socket_->connectToHost(host, port);
    return true;
}

bool NetworkSession::hostViaRelay(const QString& relayHost, quint16 relayPort, const QString& roomCode, QString& error) {
    return connectViaRelay(Mode::Host, relayHost, relayPort, roomCode, error);
}

bool NetworkSession::joinViaRelay(const QString& relayHost, quint16 relayPort, const QString& roomCode, QString& error) {
    return connectViaRelay(Mode::Client, relayHost, relayPort, roomCode, error);
}

bool NetworkSession::connectViaRelay(Mode mode, const QString& relayHost, quint16 relayPort, const QString& roomCode, QString& error) {
    if (relayHost.isEmpty()) {
        error = "Enter the relay server address.";
        return false;
    }
    if (roomCode.isEmpty()) {
        error = "Enter a room code.";
        return false;
    }

    stop();
    mode_ = mode;
    using_relay_ = true;
    pending_room_code_ = roomCode;
    socket_ = new QTcpSocket(this);
    attachSocket(socket_);
    socket_->connectToHost(relayHost, relayPort);
    return true;
}

void NetworkSession::stop() {
    if (socket_ != nullptr) {
        socket_->disconnect(this);
        socket_->close();
        socket_->deleteLater();
        socket_ = nullptr;
    }
    if (server_ != nullptr) {
        server_->close();
        server_->deleteLater();
        server_ = nullptr;
    }
    buffer_.clear();
    using_relay_ = false;
    paired_ = false;
    pending_room_code_.clear();
    mode_ = Mode::None;
}

bool NetworkSession::isConnected() const {
    if (using_relay_) {
        return paired_;
    }
    return socket_ != nullptr && socket_->state() == QAbstractSocket::ConnectedState;
}

bool NetworkSession::sendMessage(const QString& message) {
    if (!isConnected()) {
        return false;
    }
    const QByteArray payload = (message + "\n").toUtf8();
    return socket_->write(payload) == payload.size();
}

void NetworkSession::handleNewConnection() {
    if (server_ == nullptr) {
        return;
    }
    QTcpSocket* incoming = server_->nextPendingConnection();
    if (incoming == nullptr) {
        return;
    }
    if (socket_ != nullptr) {
        // Only one opponent is supported; reject additional connections.
        incoming->close();
        incoming->deleteLater();
        return;
    }

    socket_ = incoming;
    attachSocket(socket_);
    server_->close();
    emit opponentConnected();
}

void NetworkSession::handleSocketConnected() {
    if (using_relay_) {
        const QString role = mode_ == Mode::Host ? "HOST" : "CLIENT";
        socket_->write(QString("HELLO|%1|%2\n").arg(role, pending_room_code_).toUtf8());
        return;
    }
    emit opponentConnected();
}

void NetworkSession::handleReadyRead() {
    if (socket_ == nullptr) {
        return;
    }
    buffer_.append(socket_->readAll());

    if (using_relay_ && !paired_) {
        const int newline_index = buffer_.indexOf('\n');
        if (newline_index == -1) {
            return;
        }
        const QByteArray line = buffer_.left(newline_index);
        buffer_.remove(0, newline_index + 1);
        handleRelayHandshakeLine(line);
        if (!paired_) {
            return;
        }
    }

    int newline_index = buffer_.indexOf('\n');
    while (newline_index != -1) {
        const QByteArray line = buffer_.left(newline_index);
        buffer_.remove(0, newline_index + 1);
        emit messageReceived(QString::fromUtf8(line));
        newline_index = buffer_.indexOf('\n');
    }
}

void NetworkSession::handleRelayHandshakeLine(const QByteArray& line) {
    const QString text = QString::fromUtf8(line).trimmed();
    if (text == "PAIRED") {
        paired_ = true;
        emit opponentConnected();
        return;
    }

    if (text.startsWith("ERROR|")) {
        emit connectionFailed(text.section('|', 1));
        stop();
        return;
    }

    // Unexpected line before pairing; ignore it.
}

void NetworkSession::handleSocketError() {
    if (socket_ != nullptr) {
        emit connectionFailed(socket_->errorString());
    }
}

void NetworkSession::handleSocketDisconnected() {
    emit opponentDisconnected();
}

void NetworkSession::attachSocket(QTcpSocket* socket) {
    connect(socket, &QTcpSocket::connected, this, &NetworkSession::handleSocketConnected);
    connect(socket, &QTcpSocket::readyRead, this, &NetworkSession::handleReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &NetworkSession::handleSocketDisconnected);
    connect(socket, &QTcpSocket::errorOccurred, this, &NetworkSession::handleSocketError);
}

QString NetworkSession::localIPv4Address() {
    const auto addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress& address : addresses) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
            return address.toString();
        }
    }
    return "Unavailable";
}
